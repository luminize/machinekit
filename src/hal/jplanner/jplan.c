#include "rtapi.h"
#include "rtapi_app.h"
#include "rtapi_math.h"
#include "hal.h"
#include "hal_priv.h"
#include "hal_ring.h"

MODULE_AUTHOR("Michael Haberler");
MODULE_DESCRIPTION("simple joint planner");
MODULE_LICENSE("GPL");
RTAPI_TAG(HAL,HC_INSTANTIABLE);

static int comp_id;
static char *compname = "jplan";

struct inst_data {
    hal_float_t * pos_cmd;	// position command
    hal_float_t * max_vel;	// velocity limit
    hal_float_t * max_acc;	// acceleration limit

    hal_float_t *curr_pos;	// current position
    hal_float_t *curr_vel;	// current velocity
    hal_bit_t * active;		// non-zero if motion in progress
    hal_bit_t *enable;		// if zero, motion stops ASAP

};

static int update_joint(void *arg, const hal_funct_args_t *fa)
{
    struct inst_data *tp = arg;
    double period = ((double) fa_period(fa))*1e-9;
    double max_dv, tiny_dp, pos_err, vel_req;

    *(tp->active) = 0;

    /* compute max change in velocity per servo period */
    max_dv = *(tp->max_acc) * period;
    /* compute a tiny position range, to be treated as zero */
    tiny_dp = max_dv * period * 0.001;
    /* calculate desired velocity */
    if (*(tp->enable)) {
	/* planner enabled, request a velocity that tends to drive
	   pos_err to zero, but allows for stopping without position
	   overshoot */
	pos_err = *(tp->pos_cmd) - *(tp->curr_pos);
	/* positive and negative errors require some sign flipping to
	   avoid rtapi_sqrt(negative) */
	if (pos_err > tiny_dp) {
	    vel_req = -max_dv +
		       rtapi_sqrt(2.0 * *(tp->max_acc) * pos_err + max_dv * max_dv);
	    /* mark planner as active */
	    *(tp->active) = 1;
	} else if (pos_err < -tiny_dp) {
	    vel_req =  max_dv -
		       rtapi_sqrt(-2.0 * *(tp->max_acc) * pos_err + max_dv * max_dv);
	    /* mark planner as active */
	    *(tp->active) = 1;
	} else {
	    /* within 'tiny_dp' of desired pos, no need to move */
	    vel_req = 0.0;
	}
    } else {
	/* planner disabled, request zero velocity */
	vel_req = 0.0;
	/* and set command to present position to avoid movement when
	   next enabled */
	*(tp->pos_cmd) = *(tp->curr_pos);
    }
    /* limit velocity request */
    if (vel_req > *(tp->max_vel)) {
        vel_req = *(tp->max_vel);
    } else if (vel_req < -*(tp->max_vel)) {
	vel_req = -*(tp->max_vel);
    }
    /* ramp velocity toward request at accel limit */
    if (vel_req > *(tp->curr_vel) + max_dv) {
	*(tp->curr_vel) += max_dv;
    } else if (vel_req < *(tp->curr_vel) - max_dv) {
	*(tp->curr_vel) -= max_dv;
    } else {
	*(tp->curr_vel) = vel_req;
    }
    /* check for still moving */
    if (*(tp->curr_vel) != 0.0) {
	/* yes, mark planner active */
	*(tp->active) = 1;
    }
    /* integrate velocity to get new position */
    *(tp->curr_pos) += *(tp->curr_vel) * period;

    return 0;
}

static int instantiate_jplan(const char *name,
			     const int argc,
			     const char**argv)
{
    struct inst_data *ip;
    int inst_id;

    if ((inst_id = hal_inst_create(name, comp_id,
				   sizeof(struct inst_data),
				   (void **)&ip)) < 0)
	return -1;

    if (hal_pin_bit_newf(HAL_OUT, &(ip->active), inst_id, "%s.active", name) ||
	hal_pin_bit_newf(HAL_IN, &(ip->enable), inst_id, "%s.enable", name)  ||
	hal_pin_float_newf(HAL_OUT, &(ip->curr_pos), inst_id, "%s.curr-pos", name)  ||
	hal_pin_float_newf(HAL_OUT, &(ip->curr_vel), inst_id, "%s.curr-vel", name))
	return -1;

    if (hal_pin_float_newf(HAL_IN, &(ip->pos_cmd), inst_id, "%s.pos-cmd", name) ||
	hal_pin_float_newf(HAL_IO, &(ip->max_vel), inst_id, "%s.max-vel", name) ||
	hal_pin_float_newf(HAL_IO, &(ip->max_acc), inst_id, "%s.max-acc", name))
	return -1;

    hal_export_xfunct_args_t xfunct_args = {
        .type = FS_XTHREADFUNC,
        .funct.x = update_joint,
        .arg = ip,
        .uses_fp = 0,
        .reentrant = 0,
        .owner_id = inst_id
    };
    return hal_export_xfunctf(&xfunct_args, "%s.update", name);
}

static int delete_jplan(const char *name, void *inst, const int inst_size)
{
    return 0;
}

int rtapi_app_main(void)
{
    comp_id = hal_xinit(TYPE_RT, 0, 0,
			instantiate_jplan,
			delete_jplan,
			compname);
    if (comp_id < 0)
	return comp_id;
    hal_ready(comp_id);
    return 0;
}

void rtapi_app_exit(void)
{
    hal_exit(comp_id);
}
