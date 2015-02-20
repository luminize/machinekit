/*
  component written by Bas de Bruijn
  todo other info and stuff
*/

#ifndef MODULE
#include <stdint.h>
#endif
#include "rtapi.h"
#include "rtapi_app.h"
#include "rtapi_string.h"
#include "hal.h"
#include "hal_ring.h"

MODULE_AUTHOR("Bas de Bruijn");
MODULE_DESCRIPTION("Delay line component for Machinekit HAL");
MODULE_LICENSE("GPLv2");

#define MAX_INST 8
#define DEFAULT_SAMPLES 3
#define MAX_SAMPLES 9       // a pose has 9 doubles
#define DEFAULT_DELAY 1000  // 1 sec at default servo rate
#define RB_HEADROOM 1.2     // ringbuffer size overallocation

static char *names[MAX_INST] = { "delayline.0", };
RTAPI_MP_ARRAY_STRING(names, MAX_INST,"delayline names");

static int delay[MAX_INST] = {
    DEFAULT_DELAY,DEFAULT_DELAY,DEFAULT_DELAY,DEFAULT_DELAY,
    DEFAULT_DELAY,DEFAULT_DELAY,DEFAULT_DELAY,DEFAULT_DELAY
};
RTAPI_MP_ARRAY_INT(delay, MAX_INST,"number of samples to delay for each line");

static int samples[MAX_INST] = {
    DEFAULT_SAMPLES,DEFAULT_SAMPLES,DEFAULT_SAMPLES,DEFAULT_SAMPLES,
    DEFAULT_SAMPLES,DEFAULT_SAMPLES,DEFAULT_SAMPLES,DEFAULT_SAMPLES,
};
RTAPI_MP_ARRAY_INT(samples, MAX_INST,"number of pins to sample in each line");

typedef struct {
    // pins
    hal_bit_t *enable;           /* pin: enable this                   */
    hal_bit_t *abort;            /* pin: abort pin                     */
    hal_float_t *flt_in[MAX_SAMPLES];   /* pin: array incoming value          */
    hal_float_t *flt_out[MAX_SAMPLES];  /* pin: array delayed value           */
    hal_u32_t  *write_fail;      // error counter, write side
    hal_u32_t  *read_fail;       // error counter, read side
    hal_u32_t  *too_old;         // error counter, samples skipped because too old

    // other params & instance data
    uint64_t  output_ts, input_ts;
    unsigned  delay;             // delay in periods
    int nsamples;                // number of pins in this line
    size_t sample_size;          // sizeof(sample_t)  + num_channel * sizeof (hal_float_t)
    hal_bit_t last_abort;        // tracking value for edge detection
    char name[HAL_NAME_LEN + 1]; // of this instance
} hal_delayline_t;

typedef struct {
    uint64_t timestamp;          /* timestamp of measurement           */
    hal_float_t value[0];        /* measured value                     */
} sample_t;


static ringbuffer_t *instance[MAX_INST]; // instance data
static int count;              // number of instances
static int comp_id;
const char *cname = "delayline";

// thread functions
static void write_sample_to_ring(void *arg, long period);
static void read_sample_from_ring(void *arg, long period);

// this is the routine where the pins are made
static int export_delayline(int n);

// initialisation routine
int rtapi_app_main(void)
{
    int n, retval;

    // determine number of instances
    for (count = 0; (names[count] != NULL) && (count < MAX_INST); count++);

    comp_id = hal_init(cname);
    if (comp_id < 0) {
	rtapi_print_msg(RTAPI_MSG_ERR,
			"%s: ERROR: hal_init() failed, rc=%d\n", cname, comp_id);
	return -1;
    }

    // export variables and function for each DELAYLINE loop */
    for (n = 0; n < count; n++) {
	if ((retval = export_delayline(n))) {
	    rtapi_print_msg(RTAPI_MSG_ERR,
			    "%s: ERROR: loop %d var export failed\n", cname, n);
	    hal_exit(comp_id);
	    return -1;
	}
    }

    rtapi_print_msg(RTAPI_MSG_INFO,
		    "%s: installed %d lines\n", cname, count);
    hal_ready(comp_id);
    return 0;
}

// exiting routine
void rtapi_app_exit(void)
{
    hal_exit(comp_id);

    //detach from all rings, and delete them
    int n;
    for (n = 0; n < count; n++) {
	if (instance[n]) {
	    hal_delayline_t *hd = instance[n]->scratchpad;

	    char ringname[HAL_NAME_LEN + 1];
	    snprintf(ringname, HAL_NAME_LEN, "%s.samples", hd->name);
	    hal_ring_detach(ringname, instance[n]);
	    hal_ring_delete(ringname);
	}
    }
}


/* write_sample_to_ring() will take the pins with the actual value and
   put them into the ring.
*/
static void write_sample_to_ring(void *arg, long period)
{
    int j;
    ringbuffer_t *rb = (ringbuffer_t *) arg;
    hal_delayline_t *hd = rb->scratchpad;    // per-instance HAL data
    sample_t *s;

    if (!*(hd->enable)) // skip if not sampling
	goto DONE;

    // use non-copying write:
    // allocate space in the ringbuffer and retrieve a pointer to it
    if (record_write_begin(rb, (void ** )&s, hd->sample_size)) {
	*(hd->write_fail) += 1;
	goto DONE;
    }

    // deposit record directly in rb memory
    s->timestamp = hd->input_ts;
    for (j = 0; j < hd->nsamples; j++) {
	s->value[j] = *(hd->flt_in[j]);
    }

    // commit the write given the actual write size (which is the same as given in
    // record_write_begin in our case).
    // this makes the record actually visible on the read side (advances pointer)
    if (record_write_end(rb, s, hd->sample_size))
	*(hd->write_fail) += 1;

 DONE:
    hd->input_ts++;
}

// sample the pins to the current rb record
static inline void apply(const sample_t *s, const hal_delayline_t *hd)
{
    int i;
    for (i = 0; i < hd->nsamples; i++)
	*(hd->flt_out[i]) = s->value[i];
}

static void read_sample_from_ring(void *arg, long period)
{
    ringbuffer_t *rb = (ringbuffer_t *) arg;
    hal_delayline_t *hd = rb->scratchpad;
    const sample_t *s;
    size_t size;

    // detect rising edge on abort pin, and flush rb if so
    if (*(hd->abort) && (*(hd->abort) ^ hd->last_abort)) {
	int dropped = record_flush(rb);
	rtapi_print_msg(RTAPI_MSG_INFO,
			"%s: %s aborted - dropped %d samples\n",
			cname, hd->name, dropped);
    }

    // peek at the head of the queue
    while (record_read(rb, (const void **)&s, &size) == 0) {

	// do nothing if timestamp is in the future
	if (s->timestamp > hd->output_ts)
	    goto NOTYET;

	if (s->timestamp == hd->output_ts)
	    // the time is right
	    apply(s, hd);
	else
	    // skip old samples and bump an error counter
	    *(hd->too_old) += 1;

	// sanity: if (size != hd->sample_size).. terribly wrong.
	record_shift(rb); // consume record
    }
 NOTYET:
    hd->output_ts++; // always bump the timestamp
    hd->last_abort = *(hd->abort);
}

static int export_delayline(int n)
{
    int retval, i;
    char buf[HAL_NAME_LEN + 1];

    // determine the required size of the ringbuffer
    size_t nsamples =  samples[n];
    size_t sample_size = sizeof(sample_t) + (nsamples * sizeof(hal_float_t));

    // add some headroom to be sure we dont overrun
    size_t rbsize = record_space(sample_size) * delay[n] * RB_HEADROOM;

    // create the delay queue
    snprintf(buf, HAL_NAME_LEN, "%s.samples", names[n]);
    if ((retval = hal_ring_new(buf, rbsize,
			       sizeof(hal_delayline_t), ALLOC_HALMEM))) {
	rtapi_print_msg(RTAPI_MSG_ERR,
			"%s: failed to create new ring %s: %d\n",
			cname, buf, retval);
	return -1;
    }

    // use the per-using component ring access structure as the instance data,
    // which will also give us a handle on the scratchpad which we use for
    // HAL pins and other shared data
    if ((instance[n] = hal_malloc(sizeof(ringbuffer_t))) == NULL)
	return -1;
    if ((retval = hal_ring_attach(buf, instance[n], NULL))) {
	rtapi_print_msg(RTAPI_MSG_ERR,
			"%s: attach to ring %s failed: %d\n",
			cname, buf, retval);
	return -1;
    }

    // fill in instance data
    hal_delayline_t *hd = instance[n]->scratchpad;
    strncpy(hd->name, names[n], sizeof(hd->name));
    hd->nsamples = nsamples;
    hd->sample_size = sample_size;
    hd->delay = delay[n];

    hd->output_ts = 0;
    hd->input_ts =  hd->delay;

    // init pins
    for (i = 0; i < hd->nsamples; i++) {
	if (((retval = hal_pin_float_newf(HAL_IN, &(hd->flt_in[i]), comp_id,
					  "%s.in%d", hd->name, i))) ||
	    ((retval = hal_pin_float_newf(HAL_OUT, &(hd->flt_out[i]), comp_id,
					  "%s.out%d", hd->name, i))))
	    return retval;
    }
    if (((retval = hal_pin_bit_newf(HAL_IN, &(hd->enable), comp_id,
				    "%s.enable",  hd->name))) ||
	((retval = hal_pin_bit_newf(HAL_IN, &(hd->abort), comp_id,
				    "%s.abort",  hd->name))) ||
	((retval = hal_pin_u32_newf(HAL_OUT, &(hd->write_fail), comp_id,
				    "%s.write-fail", hd->name))) ||
	((retval = hal_pin_u32_newf(HAL_OUT, &(hd->read_fail), comp_id,
				    "%s.too-old", hd->name))) ||
	((retval = hal_pin_u32_newf(HAL_OUT, &(hd->too_old), comp_id,
				    "%s.read-fail", hd->name))))
	return retval;

    // export thread functions
    rtapi_snprintf(buf, sizeof(buf), "%s.sample", hd->name);
    if ((retval = hal_export_funct(buf, write_sample_to_ring,
				   instance[n], 1, 0, comp_id))) {
	return retval;
    }
    rtapi_snprintf(buf, sizeof(buf), "%s.output", hd->name);
    if ((retval = hal_export_funct(buf, read_sample_from_ring,
				   instance[n], 1, 0, comp_id))) {
	return retval;
    }
    return 0;
}
