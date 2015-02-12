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
    int *num_of_channels;        /* number of channels in this ring    */
    ringbuffer_t *ring;          /* the ring                           */
    size_t *sample_size;         /* sizeof(sample_t)
                                  + num_channel * sizeof (hal_float_t) */
} hal_ringdelay_t;

typedef struct {
    uint64_t timestamp;          /* timestamp of measurement           */
    hal_float_t value[0];        /* measured value                     */
} sample_t;

typedef enum {
  NON_CHANGING,
  INCREASING,
  DECREASING
} delay_state_t;

// pointer to array of ringdelay_t structs in shared memory, 1 per loop
static hal_ringdelay_t *ringdelay_array;
// pointer to sample
static sample_t *ring_sample;
static int comp_id;
static uint64_t actual_time = 0;

// declaration of functions for threads
static void delay(void *arg, long period);
static int write_sample_to_ring(hal_ringdelay_t * inst);
static void abort_delay(void *arg, long period);

// this is the routine where the pins are made
static int export_ringdelay(hal_ringdelay_t * inst, char * prefix, int *n);

// initialisation routine
int rtapi_app_main(void)
{
    int n, retval, i;
    int type = 0;      /* think about it as a queue */
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
    ring_sample = hal_malloc(sizeof(sample_t) + howmany_channels * sizeof(hal_float_t));
    if (ring_sample == 0) {
      rtapi_print_msg(RTAPI_MSG_ERR, "RINGDELAY: ERROR: hal_malloc() for pin_sample struct\n");
      hal_exit(comp_id);
      return -1;
    }
    /* also allocate memory for sample value array */
//    ring_sample->value = (hal_float_t **)hal_malloc(howmany_channels * sizeof(hal_float_t*));
//    if (ring_sample == 0) {
//      rtapi_print_msg(RTAPI_MSG_ERR, "RINGDELAY: ERROR: hal_malloc() for pin_sample value array\n");
//      hal_exit(comp_id);
//      return -1;
//    }

    rtapi_print_msg(RTAPI_MSG_INFO, "RINGDELAY: memory allocated for pin_sample\n");

    /* export variables and function for each RINGDELAY loop */
    i = 0; // for names= items
    for (n = 0; n < howmany_delays; n++) {
      /* create ring*/
      snprintf(ringname, HAL_NAME_LEN, "ringdelay-ring%d",n);
      retval = hal_ring_new(ringname, single_ring_size, spsize,type);
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
/*******************************************************************************
see src/rtapi/ring.h

the writing to the delay buffer works as follows:
1. record_write_begin() will be called to give a pointer to cast to. This
   function returns 0 if there is enough size to write to. the size is:
   uint64_t + (channel * hal_float_t)
2. write the data to the pointer given by record_write_begin()
3. then finish the write by calling record_write_end.

!  when the sample is written, the timestamp is the time at the time of writing
   this will later be read to determine delay times, and the choice of
   in/decreasing reading and/or writing records

the reading from the delay buffer works as follows:
1. record_read() will be called to give a pointer to the current record.
2. the data the pointer references to then can be dealt with, considered, and
   acted upon. like delayed_sample->value[n] and delayed_sample->timestamp

advancing the reading pointer is done by:
1. calling record_shift() which advances the pointer to the next one.

typically we can have 3 states:
1: non-changing delay time
2: increasing delay time
3: decreasing delay time

needed delay time vs current delay time

- the requested `delay_time` is the time measured at the input pin. example: 12
- the `curr_delay_time` is calculated from the time stamp in the current
  read record and the current time, say timestamp = 45 and time is 50; then:
  delay = 50 - 45 = 5

behaviour:
a. if `delay_time` == `curr_delay_time`
   write sample to ring
   read sample from ring
b. if `delay_time` > `curr_delay_time`
   write sample to ring
   do not record_shift()
c. if `delay_time` < `curr_delay_time`
   do not write a new record to ring
   read sample from ring
   set pins etc.

*******************************************************************************/
static void delay(void *arg, long period)
{
    sample_t readsample, writesample;
    ringbuffer_t *rb;
    hal_ringdelay_t *ringdelay;
    size_t *size;
    delay_state_t delay_state;
    ringdelay = arg;
    /* the very first time, during initialiszation the first value with a
       timestamp of "0" has been written to the ring. Because we need data to
       read some info from the read position.

       the reason we want to start with the reading before the writing is that
       reading the timestamp will allow us to make the decision about consuming
       the record and writing a new record to the ring */

    /* decide upon which delay state we are */

    actual_time++;
}

/* write_sample_to_ring() will take the pins with the actual value and
   put them into the ring.
*/
static int write_sample_to_ring(hal_ringdelay_t * inst)
{
    int retval, j;
    ring_sample->timestamp = actual_time;
    for (j = 0; j < *(inst->num_of_channels); j++) {
      ring_sample->value[j] = *(inst->flt_in[j]);
    }
    retval = record_write_begin(inst->ring, ring_sample, *(inst->sample_size));
    if (retval != 0) {
        rtapi_print_msg(RTAPI_MSG_ERR,
          "RINGDELAY: %s: ERROR: something went wrong with record_write_begin, returncode: %d\n",
          comp_id, retval);
    }

    else{
        retval = record_write_end(inst->ring, ring_sample, *(inst->sample_size));
        if (retval != 0) {
            rtapi_print_msg(RTAPI_MSG_ERR,
              "RINGDELAY: %s: ERROR: something went wrong with record_write_end, returncode: %d\n",
              comp_id, retval);
        }
    }
    if (retval == 0) {
        return 0;
    }
    else {
        rtapi_print_msg(RTAPI_MSG_ERR,
        "RINGDELAY: %s: ERROR: write_sample_to_ring() failed: %d\n",
          comp_id, retval);
        hal_exit(comp_id);
        return -1;
    }
}

static int read_sample_from_ring(hal_ringdelay_t * inst)
{

}

// export these pins so they exist in the HAL layer
static int export_ringdelay(hal_ringdelay_t * inst, char * prefix, int * n)
{
    int retval, i, msg;
    char buf[HAL_NAME_LEN + 1];
    char ringname[HAL_NAME_LEN + 1];
    msg = rtapi_get_msg_level();
    rtapi_set_msg_level(RTAPI_MSG_WARN);

    /* store number of channels in this instance */
    *(inst->num_of_channels) = howmany_channels;
    /* store size of sample_t in instance */
    *(inst->sample_size) = sizeof(sample_t) + (*(inst->num_of_channels) * sizeof(hal_float_t));
    if (inst->sample_size == 0) {
      rtapi_print_msg(RTAPI_MSG_ERR, "RINGDELAY: ERROR: wrong instance sample size calculated\n");
      hal_exit(comp_id);
      return -1;
    }

    /* for each channel in this delay the pins must be set*/
    /* example from matrix_kb.c
    ** inst->hal.key = (hal_bit_t **)hal_malloc(inst->nrows * inst->ncols * sizeof(hal_bit_t*));
    */
    inst->flt_in = (hal_float_t **)hal_malloc(*(inst->num_of_channels) * sizeof(hal_float_t*));
      if (inst->flt_in == 0) {
        rtapi_print_msg(RTAPI_MSG_ERR, "RINGDELAY: ERROR: hal_malloc() for ringdelay_array[n]->flt_in failed\n");
        hal_exit(comp_id);
        return -1;
      }
    inst->flt_out = (hal_float_t **)hal_malloc(*(inst->num_of_channels) * sizeof(hal_float_t*));
      if (inst->flt_out == 0) {
        rtapi_print_msg(RTAPI_MSG_ERR, "RINGDELAY: ERROR: hal_malloc() for ringdelay_array[n]->flt_out failed\n");
        hal_exit(comp_id);
        return -1;
      }
    /* memory for pin-array succesfully allocated */
    rtapi_print_msg(RTAPI_MSG_ERR,
      "RINGDELAY: memory for pins %s.in and %s.out allocated", prefix, prefix);
    /* set up the array of pins */
    for (i = 0; i < *(inst->num_of_channels); i++) {
      retval = hal_pin_float_newf(HAL_IN, &(inst->flt_in[i]), comp_id,
          "%s.in%d", prefix, i);
      if (retval != 0) {
        return retval;
      }
      retval = hal_pin_float_newf(HAL_OUT, &(inst->flt_out[i]), comp_id,
          "%s.out%d", prefix, i);
      if (retval != 0) {
        return retval;
      }
      /* give the pins initial values */
      *(inst->flt_in[i]) = 0.0;
      *(inst->flt_out[i]) = 0.0;
    }
    retval = hal_pin_bit_newf(HAL_IN, &(inst->bit_enable), comp_id,
            "%s.enable", prefix);
    if (retval != 0) {
      return retval;
    }
    retval = hal_pin_bit_newf(HAL_OUT, &(inst->bit_abort), comp_id,
        "%s.abort", prefix);
    if (retval != 0) {
      return retval;
    }
    retval = hal_pin_u32_newf(HAL_IN, &(inst->delay), comp_id,
        "%s.delay", prefix);
    if (retval != 0) {
      return retval;
    }
    retval = hal_pin_float_newf(HAL_OUT, &(inst->dbg_i_read), comp_id,
        "%s.dbg-i-read", prefix);
    if (retval != 0) {
      return retval;
    }
    retval = hal_pin_float_newf(HAL_OUT, &(inst->dbg_i_write), comp_id,
        "%s.dbg-i-write", prefix);
    if (retval != 0) {
      return retval;
    }

    /* set the initial values*/
    *(inst->bit_enable) = 0;
    *(inst->bit_abort) = 0;
    *(inst->delay) = 0;

    // export the functions for using in the (servo-)thread
    /* delayed_write function */
//    rtapi_snprintf(buf, sizeof(buf), "%s.write-output", prefix);
//    retval = hal_export_funct(buf, delayed_write, inst, 1, 0, comp_id);
//    if (retval != 0) {
//      rtapi_print_msg(RTAPI_MSG_ERR,
//        "RINGDELAY: ERROR: delayed_write function export failed\n");
//      hal_exit(comp_id);
//      return -1;
//    }
    /* calculate_delay function */
    rtapi_snprintf(buf, sizeof(buf), "%s.delay", prefix);
    retval = hal_export_funct(buf, delay, inst, 1, 0, comp_id);
    if (retval != 0) {
      rtapi_print_msg(RTAPI_MSG_ERR,
        "RINGDELAY: ERROR: calculate_delay function export failed\n");
      hal_exit(comp_id);
      return -1;
    }
    /* attach to the newly created ring*/
    inst->ring = hal_malloc(sizeof(ringbuffer_t));
    snprintf(ringname, HAL_NAME_LEN, "ringdelay-ring%d",*n);
    retval = hal_ring_attach(ringname, inst->ring,NULL);
    if (retval != 0) {
        rtapi_print_msg(RTAPI_MSG_ERR,
        "RINGDELAY: %s: ERROR: hal_ring_attach() failed: %d\n",
          ringname, retval);
        hal_exit(comp_id);
    return -1;
    }
    /* make a very first write to have something in the buffer to read from */
    retval = write_sample_to_ring(inst);
    /* after this moment there is a record in the ring, with timestamp zero */

    /* restore saved message level */
    rtapi_set_msg_level(msg);
    return 0;
}
