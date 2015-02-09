/*
component written by Bas de Bruijn
todo other info and stuff
*/


#include <stdint.h>
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

#define MAX_DELAYS 8
char *delays[MAX_DELAYS];
static int num_delay;	 	        /* number of delays */
static int default_num_delay = 1;
RTAPI_MP_INT(num_delay, "number of delays");
static int howmany_delays;

#define MAX_CHANNELS 8
char *channels[MAX_CHANNELS];
static int num_channel;          /* number of chanels per delay */
static int default_num_channel = 3;
RTAPI_MP_INT(num_channel, "nr of channels for a delay");
static int howmany_channels;

char *names[MAX_CHANNELS] ={0,};
RTAPI_MP_ARRAY_STRING(names, MAX_CHANNELS,"ringdelay names");

/* ringbuffer stuff, including the single_ring_size when loading */
static int single_ring_size;
static int default_ring_size = 1024;
RTAPI_MP_INT(single_ring_size, "single ringdelay size");
static int spsize = 0;
RTAPI_MP_INT(spsize, "size of scratchpad area");
static int in_halmem = 1;
RTAPI_MP_INT(in_halmem, "allocate ring in HAL shared memory");

static char *command = "command";
RTAPI_MP_STRING(command,  "name of command ring");

typedef struct {
    hal_bit_t *bit_enable;       /* pin: enable this                   */
    hal_bit_t *bit_abort;        /* pin: abort pin                     */
    hal_float_t **flt_in;	      /* pin: array incoming value          */
    hal_float_t **flt_out;	     /* pin: array delayed value           */
    hal_u32_t *delay;            /* pin: delay time                    */
    hal_float_t *dbg_i_read;	   /* pin: debug pin for read position   */
    hal_float_t *dbg_i_write;	  /* pin: debug pin for write position  */
    ringbuffer_t *ring;          /* the ring                           */
} hal_ringdelay_t;

typedef struct {
    uint64_t timestamp;          /* timestamp of measurement           */
    hal_float_t value[0];        /* measured value                     */
} sample_t;

// pointer to array of ringdelay_t structs in shared memory, 1 per loop
static hal_ringdelay_t *ringdelay_array;
// pointer to sample
static sample_t *pin_sample;
static int comp_id;

// declaration of functions for threads
static void read_input(void *arg, long period);
static void calculate_delay(void *arg, long period);
static void delayed_write(void *arg, long period);
static void abort_delay(void *arg, long period);

// this is the routine where the pins are made
static int export_ringdelay(hal_ringdelay_t * addr, char * prefix, int *n);

// initialisation routine
int rtapi_app_main(void)
{
    int n, retval, i;
    int circular_buffer = 1;      /* circular buffer of non-zero  */
    char ringname[HAL_NAME_LEN + 1];
    hal_ringdelay_t *current_ringdelay;
    if(num_delay && names[0]) {
      rtapi_print_msg(RTAPI_MSG_ERR,"num_delay= and names= are mutually exclusive\n");
    return -EINVAL;
    }
    if(!num_delay && !names[0]) num_delay = default_num_delay;

    if(num_delay) {
      howmany_delays = num_delay;
    }
    else {
      howmany_delays = 0;
      for (i = 0; i < MAX_DELAYS; i++) {
          if (names[i] == NULL) {
              break;
          }
          howmany_delays = i + 1;
      }
    }
    /* see if a single_ring_size has been given */
    if(!single_ring_size) {
      single_ring_size = default_ring_size;
    }
    /* check number of channels per delay*/
    if(!num_channel) {
      num_channel = default_num_channel;
    }
    if(num_channel) {
      howmany_channels = num_channel;
    }
    /* test for number of delays */
    if ((howmany_delays <= 0) || (howmany_delays > MAX_DELAYS)) {
      rtapi_print_msg(RTAPI_MSG_ERR,
        "RINGDELAY: ERROR: invalid number of delays: %d\n", howmany_delays);
      return -1;
    }
      /* test for number of channels */
    if ((howmany_channels <= 0) || (howmany_channels > MAX_CHANNELS)) {
      rtapi_print_msg(RTAPI_MSG_ERR,
        "RINGDELAY: ERROR: invalid number of chanels: %d\n", howmany_channels);
      return -1;
    }
    rtapi_print_msg(RTAPI_MSG_INFO, "RINGDELAY: %d delays with %d channels\n",
      howmany_delays, howmany_channels);
    /* have good config info, connect to the HAL */
    comp_id = hal_init("ringdelay");
    if (comp_id < 0) {
      rtapi_print_msg(RTAPI_MSG_ERR, "RINGDELAY: ERROR: hal_init() failed\n");
      return -1;
    }
    rtapi_print_msg(RTAPI_MSG_INFO, "RINGDELAY: connected to HAL\n");

    /* allocate shared memory for RINGDELAY instances */
    ringdelay_array = hal_malloc(howmany_delays * sizeof(hal_ringdelay_t));
    if (ringdelay_array == 0) {
      rtapi_print_msg(RTAPI_MSG_ERR, "RINGDELAY: ERROR: hal_malloc() for hal_ringdelay_t failed\n");
      hal_exit(comp_id);
      return -1;
    }
    rtapi_print_msg(RTAPI_MSG_INFO, "RINGDELAY: memory allocated for ringdelay_array\n");
    /* allocate shared memory for SAMPLE data */
    pin_sample = hal_malloc(sizeof(sample_t));
    if (pin_sample == 0) {
      rtapi_print_msg(RTAPI_MSG_ERR, "RINGDELAY: ERROR: hal_malloc() for pin_sample failed\n");
      hal_exit(comp_id);
      return -1;
    }
    rtapi_print_msg(RTAPI_MSG_INFO, "RINGDELAY: memory allocated for pin_sample\n");

    /* export variables and function for each RINGDELAY loop */
    i = 0; // for names= items
    for (n = 0; n < howmany_delays; n++) {
      /* create ring*/
      snprintf(ringname, HAL_NAME_LEN, "ringdelay-ring%d",n);
      retval = hal_ring_new(ringname, single_ring_size, spsize,circular_buffer);
      if (retval != 0) {
          rtapi_print_msg(RTAPI_MSG_ERR,
              "RINGDELAY: failed to create new ring %s: %d\n",
              ringname, retval);
        }
      /* export the pins */
      if(num_delay) {
        char buf[HAL_NAME_LEN + 1];
        rtapi_snprintf(buf, sizeof(buf), "ringdelay.%d", n);
        retval = export_ringdelay(&(ringdelay_array[n]), buf, &n);
      }
      else {
        retval = export_ringdelay(&(ringdelay_array[n]), names[i++], &n);
      }

      if (retval != 0) {
        rtapi_print_msg(RTAPI_MSG_ERR,
          "RINGDELAY: ERROR: loop %d var export failed\n", n);
        hal_exit(comp_id);
        return -1;
      }
    }

    rtapi_print_msg(RTAPI_MSG_INFO,
      "RINGDELAY: installed %d RINGDELAY loops\n", howmany_delays);
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
static int export_ringdelay(hal_ringdelay_t * addr, char * prefix, int * n)
{
    int retval, i, msg;
    char buf[HAL_NAME_LEN + 1];
    char ringname[HAL_NAME_LEN + 1];
    msg = rtapi_get_msg_level();
    rtapi_set_msg_level(RTAPI_MSG_WARN);

    /* for each channel in this delay the pins must be set*/
    /* example from matrix_kb.c
    ** inst->hal.key = (hal_bit_t **)hal_malloc(inst->nrows * inst->ncols * sizeof(hal_bit_t*));
    */
    addr->flt_in = (hal_float_t **)hal_malloc(num_channel * sizeof(hal_float_t*));
      if (addr->flt_in == 0) {
        rtapi_print_msg(RTAPI_MSG_ERR, "RINGDELAY: ERROR: hal_malloc() for ringdelay_array[n]->flt_in failed\n");
        hal_exit(comp_id);
        return -1;
      }
    addr->flt_out = (hal_float_t **)hal_malloc(num_channel * sizeof(hal_float_t*));
      if (addr->flt_out == 0) {
        rtapi_print_msg(RTAPI_MSG_ERR, "RINGDELAY: ERROR: hal_malloc() for ringdelay_array[n]->flt_out failed\n");
        hal_exit(comp_id);
        return -1;
      }
    /* memory for pin-array succesfully allocated */
    rtapi_print_msg(RTAPI_MSG_ERR,
      "RINGDELAY: memory for pins %s.in and %s.out allocated", prefix, prefix);
    /* set up the array of pins */
    for (i = 0; i < num_channel; i++) {
      retval = hal_pin_float_newf(HAL_IN, &(addr->flt_in[i]), comp_id,
          "%s.in%d", prefix, i);
      if (retval != 0) {
        return retval;
      }
      retval = hal_pin_float_newf(HAL_OUT, &(addr->flt_out[i]), comp_id,
          "%s.out%d", prefix, i);
      if (retval != 0) {
        return retval;
      }
      /* give the pins initial values */
      *(addr->flt_in[i]) = 0.0;
      *(addr->flt_out[i]) = 0.0;
    }
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
    retval = hal_pin_u32_newf(HAL_IN, &(addr->delay), comp_id,
        "%s.delay", prefix);
    if (retval != 0) {
      return retval;
    }
    retval = hal_pin_float_newf(HAL_OUT, &(addr->dbg_i_read), comp_id,
        "%s.dbg-i-read", prefix);
    if (retval != 0) {
      return retval;
    }
    retval = hal_pin_float_newf(HAL_OUT, &(addr->dbg_i_write), comp_id,
        "%s.dbg-i-write", prefix);
    if (retval != 0) {
      return retval;
    }

    /* set the initial values*/
    *(addr->bit_enable) = 0;
    *(addr->bit_abort) = 0;
    *(addr->delay) = 0;

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
    /* attach to the newly created ring*/
    addr->ring = hal_malloc(sizeof(ringbuffer_t));
    snprintf(ringname, HAL_NAME_LEN, "ringdelay-ring%d",*n);
    retval = hal_ring_attach(ringname, addr->ring,NULL);
    if (retval != 0) {
        rtapi_print_msg(RTAPI_MSG_ERR,
        "RINGDELAY: %s: ERROR: hal_ring_attach() failed: %d\n",
      ringname, retval);
    return -1;
      }
    /* restore saved message level */
    rtapi_set_msg_level(msg);
    return 0;
}
