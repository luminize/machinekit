/*
component written by Bas de Bruijn
todo other info and stuff
*/

#include "rtapi.h"
#include "rtapi_app.h"
#include "rtapi_string.h"
#include "hal.h"
#include "hal_priv.h"
#include "hal_ring.h"

/* module information */
MODULE_AUTHOR("Bas de Bruijn");
MODULE_DESCRIPTION("Ringbuffer delay component for Machinekit HAL");
MODULE_LICENSE("GPLv2");
static int num_chan;		        /* number of channels */
static int default_num_chan = 3;
RTAPI_MP_INT(num_chan, "number of channels");

static int howmany;
#define MAX_CHAN 16
char *names[MAX_CHAN] ={0,};
RTAPI_MP_ARRAY_STRING(names, MAX_CHAN,"ringdelay names");

/* ringbuffer stuff, including the single_ring_size when loading */
static int single_ring_size;
static int default_ring_size = 1024;
RTAPI_MP_INT(single_ring_size, "single ringdelay size");
static int spsize = 0;
RTAPI_MP_INT(spsize, "size of scratchpad area");
static int in_halmem = 1;
RTAPI_MP_INT(in_halmem, "allocate ring in HAL shared memory");

typedef struct {
    hal_bit_t *bit_enable;       /* pin: enable this */
    hal_bit_t *bit_abort;        /* pin: abort pin */
    hal_float_t *flt_in;	       /* pin: incoming value */
    hal_float_t *flt_out;	      /* pin: delayed value */
    hal_float_t *flt_delay;      /* pin: delay time */
    hal_ring_t *ring;            /* the ring */
} hal_ringdelay_t;

// pointer to array of ringdelay_t structs in shared memory, 1 per loop
static hal_ringdelay_t *ringdelay_array;
static int comp_id;

// declaration of functions for threads
static void read_input(void *arg, long period);
static void calculate_delay(void *arg, long period);
static void delayed_write(void *arg, long period);
static void abort_delay(void *arg, long period);

// this is the routine where the pins are made
static int export_ringdelay(hal_ringdelay_t * addr,char * prefix);

// initialisation routine
int rtapi_app_main(void)
{
    int n, retval,i;

    if(num_chan && names[0]) {
      rtapi_print_msg(RTAPI_MSG_ERR,"num_chan= and names= are mutually exclusive\n");
    return -EINVAL;
    }
    if(!num_chan && !names[0]) num_chan = default_num_chan;

    if(num_chan) {
      howmany = num_chan;
    }
    else {
      howmany = 0;
      for (i = 0; i < MAX_CHAN; i++) {
          if (names[i] == NULL) {
              break;
          }
          howmany = i + 1;
      }
    }
    /* see if a single_ring_size has been given */
    if(!single_ring_size) {
      single_ring_size = default_ring_size;
    }
    /* test for number of channels */
    if ((howmany <= 0) || (howmany > MAX_CHAN)) {
      rtapi_print_msg(RTAPI_MSG_ERR,
        "RINGDELAY: ERROR: invalid number of channels: %d\n", howmany);
      return -1;
    }

    /* have good config info, connect to the HAL */
    comp_id = hal_init("ringdelay");
    if (comp_id < 0) {
      rtapi_print_msg(RTAPI_MSG_ERR, "RINGDELAY: ERROR: hal_init() failed\n");
      return -1;
    }

    /* allocate shared memory for RINGDELAY loop data */
    ringdelay_array = hal_malloc(howmany * sizeof(hal_ringdelay_t));
    if (ringdelay_array == 0) {
      rtapi_print_msg(RTAPI_MSG_ERR, "RINGDELAY: ERROR: hal_malloc() failed\n");
      hal_exit(comp_id);
      return -1;
    }

    /* export variables and function for each RINGDELAY loop */
    i = 0; // for names= items
    for (n = 0; n < howmany; n++) {

      if(num_chan) {
        char buf[HAL_NAME_LEN + 1];
        rtapi_snprintf(buf, sizeof(buf), "ringdelay.%d", n);
        retval = export_ringdelay(&(ringdelay_array[n]), buf);
      }
      else {
        retval = export_ringdelay(&(ringdelay_array[n]), names[i++]);
      }

      if (retval != 0) {
        rtapi_print_msg(RTAPI_MSG_ERR,
          "RINGDELAY: ERROR: loop %d var export failed\n", n);
        hal_exit(comp_id);
        return -1;
      }
    }

    rtapi_print_msg(RTAPI_MSG_INFO,
      "RINGDELAY: installed %d RINGDELAY loops\n", howmany);
    hal_ready(comp_id);
    return 0;
}

// exiting routine
void rtapi_app_exit(void)
{
    hal_exit(comp_id);
}

// abort routine for if something goes horribly wrong
static void abort_delay(void *arg, long period)
{

}

/* the read_input routine will read the input pin and decides
   if it should write this to the ringbuffer. This decision
   is based on the fact if we want to decreade the time

   this decreasing can only happen from a safe situation like
   a situation where there is no motion for example
   this is where the bit_is_safe comes in. that should be connected
   to motion.current-motion for example
*/
static void read_input(void *arg, long period)
{

}

static void calculate_delay(void *arg, long period)
{

}

/* the delayed_write routine decides to read the "delayed" pointer
   but only if some constraints are met. If it would do
   so every servo period, then there would never be a delay

   so first lead the pointer information without changing
   and then decide if we want to read for real
*/
static void delayed_write(void *arg, long period)
{

}

// export these pins so they exist in the HAL layer
static int export_ringdelay(hal_ringdelay_t * addr, char * prefix)
{
    int retval, msg;
    char buf[HAL_NAME_LEN + 1];
    int circular_buffer = 1;      /* circular buffer of non-zero  */
    msg = rtapi_get_msg_level();
    rtapi_set_msg_level(RTAPI_MSG_WARN);

    retval = hal_pin_bit_newf(HAL_IN, &(addr->bit_enable), comp_id,
            "%s.enable", prefix);
    if (retval != 0) {
      return retval;
    }
    retval = hal_pin_bit_newf(HAL_OUT, &(addr->bit_abort), comp_id,
        "%s.abort", prefix);
    if (retval != 0) {
      return retval;
    }
    retval = hal_pin_float_newf(HAL_IN, &(addr->flt_in), comp_id,
        "%s.in", prefix);
    if (retval != 0) {
      return retval;
    }
    retval = hal_pin_float_newf(HAL_OUT, &(addr->flt_out), comp_id,
        "%s.out", prefix);
    if (retval != 0) {
      return retval;
    }
    retval = hal_pin_float_newf(HAL_IN, &(addr->flt_delay), comp_id,
        "%s.delay", prefix);
    if (retval != 0) {
      return retval;
    }

    /* set the initial values*/
    *(addr->bit_enable) = 0;
    *(addr->bit_abort) = 0;
    *(addr->flt_in) = 0.0;
    *(addr->flt_out) = 0.0;
    *(addr->flt_delay) = 0;

    // export the functions for using in the (servo-)thread
    // read_input function
    rtapi_snprintf(buf, sizeof(buf), "%s.read-input", prefix);
    retval = hal_export_funct(buf, read_input, addr, 1, 0, comp_id);
    if (retval != 0) {
      rtapi_print_msg(RTAPI_MSG_ERR,
        "RINGDELAY: ERROR: read_input function export failed\n");
      hal_exit(comp_id);
      return -1;
    }
    /* delayed_write function */
    rtapi_snprintf(buf, sizeof(buf), "%s.write-output", prefix);
    retval = hal_export_funct(buf, delayed_write, addr, 1, 0, comp_id);
    if (retval != 0) {
      rtapi_print_msg(RTAPI_MSG_ERR,
        "RINGDELAY: ERROR: delayed_write function export failed\n");
      hal_exit(comp_id);
      return -1;
    }
    /* calculate_delay function */
    rtapi_snprintf(buf, sizeof(buf), "%s.calculate-delay", prefix);
    retval = hal_export_funct(buf, calculate_delay, addr, 1, 0, comp_id);
    if (retval != 0) {
      rtapi_print_msg(RTAPI_MSG_ERR,
        "RINGDELAY: ERROR: calculate_delay function export failed\n");
      hal_exit(comp_id);
      return -1;
    }
    /* make a new ring, like ringdelay.0.ring */
    rtapi_snprintf(buf, sizeof(buf), "%s.ring", prefix);
    retval = hal_ring_new(buf, single_ring_size, spsize, circular_buffer);
    if (retval != 0) {
      rtapi_print_msg(RTAPI_MSG_ERR,
        "RINGDELAY ERROR: failed to create new ring %s.ring\n",
        buf, retval);
      return -1;
    }
    /* restore saved message level */
    rtapi_set_msg_level(msg);
    return 0;
}
