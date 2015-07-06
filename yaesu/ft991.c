/*
 * hamlib - (C) Frank Singleton 2000 (javabear at users.sourceforge.net)
 *
 * ft991.c - (C) Nate Bargmann 2007 (n0nb at arrl.net)
 *           (C) Stephane Fillod 2008
 *           (C) Terry Embry 2008-2009
 *           (C) Michael Black W9MDB 2015 -- taken from ft950.c
 *
 * The FT991 is very much like the FT950 except freq max increases for 440MHz
 * So most of this code is a duplicate of the FT950
 *
 * This shared library provides an API for communicating
 * via serial interface to an FT-991 using the "CAT" interface
 *
 *
 *   This library is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU Lesser General Public
 *   License as published by the Free Software Foundation; either
 *   version 2.1 of the License, or (at your option) any later version.
 *
 *   This library is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *   Lesser General Public License for more details.
 *
 *   You should have received a copy of the GNU Lesser General Public
 *   License along with this library; if not, write to the Free Software
 *   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */


#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>
#include "hamlib/rig.h"
#include "bandplan.h"
#include "serial.h"
#include "misc.h"
#include "yaesu.h"
#include "newcat.h"
#include "ft991.h"
#include "idx_builtin.h"

/*
 * ft991 rigs capabilities.
 * Also this struct is READONLY!
 *
 */

const struct rig_caps ft991_caps = {
    .rig_model =          RIG_MODEL_FT991,
    .model_name =         "FT-991",
    .mfg_name =           "Yaesu",
    .version =            NEWCAT_VER ".2",
    .copyright =          "LGPL",
    .status =             RIG_STATUS_BETA,
    .rig_type =           RIG_TYPE_TRANSCEIVER,
    .ptt_type =           RIG_PTT_RIG,
    .dcd_type =           RIG_DCD_NONE,
    .port_type =          RIG_PORT_SERIAL,
    .serial_rate_min =    4800,         /* Default rate per manual */
    .serial_rate_max =    38400,
    .serial_data_bits =   8,
    .serial_stop_bits =   1,            /* Assumed since manual makes no mention */
    .serial_parity =      RIG_PARITY_NONE,
    .serial_handshake =   RIG_HANDSHAKE_HARDWARE,
    .write_delay =        FT991_WRITE_DELAY,
    .post_write_delay =   FT991_POST_WRITE_DELAY,
    .timeout =            2000,
    .retry =              3,
    .has_get_func =       FT991_FUNCS,
    .has_set_func =       FT991_FUNCS,
    .has_get_level =      FT991_LEVELS,
    .has_set_level =      RIG_LEVEL_SET(FT991_LEVELS),
    .has_get_parm =       RIG_PARM_NONE,
    .has_set_parm =       RIG_PARM_NONE,
    .level_gran = {
	[LVL_RAWSTR] = { .min = { .i = 0 }, .max = { .i = 255 } },
	[LVL_CWPITCH] = { .min = { .i = 300 }, .max = { .i = 1050 }, .step = { .i = 50 } },
    },
    .ctcss_list =         common_ctcss_list,
    .dcs_list =           NULL,
    .preamp =             { 10, 20, RIG_DBLST_END, }, /* TBC */
    .attenuator =         { 6, 12, 18, RIG_DBLST_END, },
    .max_rit =            Hz(9999),
    .max_xit =            Hz(9999),
    .max_ifshift =        Hz(1000),
    .vfo_ops =            FT991_VFO_OPS,
    .targetable_vfo =     RIG_TARGETABLE_FREQ,
    .transceive =         RIG_TRN_OFF,        /* May enable later as the 950 has an Auto Info command */
    .bank_qty =           0,
    .chan_desc_sz =       0,
    .str_cal =            FT991_STR_CAL,
    .chan_list =          {
               {   1,  99, RIG_MTYPE_MEM,  NEWCAT_MEM_CAP },
               { 100, 117, RIG_MTYPE_EDGE, NEWCAT_MEM_CAP },    /* two by two */
               RIG_CHAN_END,
                          },

    .rx_range_list1 =     {
        {kHz(30), MHz(470), FT991_ALL_RX_MODES, -1, -1, FT991_VFO_ALL, FT991_ANTS},   /* General coverage + ham */
        RIG_FRNG_END,
    }, /* FIXME:  Are these the correct Region 1 values? */

    .tx_range_list1 =     {
        FRQ_RNG_HF(1, FT991_OTHER_TX_MODES, W(5), W(100), FT991_VFO_ALL, FT991_ANTS),
        FRQ_RNG_HF(1, FT991_AM_TX_MODES, W(2), W(25), FT991_VFO_ALL, FT991_ANTS),	/* AM class */
        FRQ_RNG_6m(1, FT991_OTHER_TX_MODES, W(5), W(100), FT991_VFO_ALL, FT991_ANTS),
        FRQ_RNG_6m(1, FT991_AM_TX_MODES, W(2), W(25), FT991_VFO_ALL, FT991_ANTS),	/* AM class */

        RIG_FRNG_END,
    },

    .rx_range_list2 =     {
        {kHz(30), MHz(470), FT991_ALL_RX_MODES, -1, -1, FT991_VFO_ALL, FT991_ANTS},
        RIG_FRNG_END,
    },

    .tx_range_list2 =     {
        FRQ_RNG_HF(2, FT991_OTHER_TX_MODES, W(5), W(100), FT991_VFO_ALL, FT991_ANTS),
        FRQ_RNG_HF(2, FT991_AM_TX_MODES, W(2), W(25), FT991_VFO_ALL, FT991_ANTS),	/* AM class */
        FRQ_RNG_6m(2, FT991_OTHER_TX_MODES, W(5), W(100), FT991_VFO_ALL, FT991_ANTS),
        FRQ_RNG_6m(2, FT991_AM_TX_MODES, W(2), W(25), FT991_VFO_ALL, FT991_ANTS),	/* AM class */

        RIG_FRNG_END,
    },

    .tuning_steps =       {
        {FT991_SSB_CW_RX_MODES, Hz(10)},    /* Normal */
        {FT991_SSB_CW_RX_MODES, Hz(100)},   /* Fast */

        {FT991_AM_RX_MODES,     Hz(100)},   /* Normal */
        {FT991_AM_RX_MODES,     kHz(1)},    /* Fast */

        {FT991_FM_RX_MODES,     Hz(100)},   /* Normal */
        {FT991_FM_RX_MODES,     kHz(1)},    /* Fast */

        RIG_TS_END,

    },

    /* mode/filter list, .remember =  order matters! */
 .filters =            {
        {FT991_CW_RTTY_PKT_RX_MODES,  Hz(1700)},    /* Normal CW, RTTY, PKT */
        {FT991_CW_RTTY_PKT_RX_MODES,  Hz(500)},     /* Narrow CW, RTTY, PKT */
        {FT991_CW_RTTY_PKT_RX_MODES,  Hz(2400)},    /* Wide   CW, RTTY, PKT */
        {FT991_CW_RTTY_PKT_RX_MODES,  Hz(2000)},    /*        CW, RTTY, PKT */
        {FT991_CW_RTTY_PKT_RX_MODES,  Hz(1400)},    /*        CW, RTTY, PKT */
        {FT991_CW_RTTY_PKT_RX_MODES,  Hz(1200)},    /*        CW, RTTY, PKT */
        {FT991_CW_RTTY_PKT_RX_MODES,  Hz(800)},     /*        CW, RTTY, PKT */
        {FT991_CW_RTTY_PKT_RX_MODES,  Hz(400)},     /*        CW, RTTY, PKT */
        {FT991_CW_RTTY_PKT_RX_MODES,  Hz(300)},     /*        CW, RTTY, PKT */
        {FT991_CW_RTTY_PKT_RX_MODES,  Hz(200)},     /*        CW, RTTY, PKT */
        {FT991_CW_RTTY_PKT_RX_MODES,  Hz(100)},     /*        CW, RTTY, PKT */
        {RIG_MODE_SSB,                Hz(2400)},    /* Normal SSB */
        {RIG_MODE_SSB,                Hz(1800)},    /* Narrow SSB */
        {RIG_MODE_SSB,                Hz(3000)},    /* Wide   SSB */
        {RIG_MODE_SSB,                Hz(2900)},    /*        SSB */
        {RIG_MODE_SSB,                Hz(2800)},    /*        SSB */
        {RIG_MODE_SSB,                Hz(2700)},    /*        SSB */
        {RIG_MODE_SSB,                Hz(2600)},    /*        SSB */
        {RIG_MODE_SSB,                Hz(2500)},    /*        SSB */
        {RIG_MODE_SSB,                Hz(2250)},    /*        SSB */
        {RIG_MODE_SSB,                Hz(2100)},    /*        SSB */
        {RIG_MODE_SSB,                Hz(1950)},    /*        SSB */
        {RIG_MODE_SSB,                Hz(1650)},    /*        SSB */
        {RIG_MODE_SSB,                Hz(1500)},    /*        SSB */
        {RIG_MODE_SSB,                Hz(1350)},    /*        SSB */
        {RIG_MODE_SSB,                Hz(1100)},    /*        SSB */
        {RIG_MODE_SSB,                Hz(850)},     /*        SSB */
        {RIG_MODE_SSB,                Hz(600)},     /*        SSB */
        {RIG_MODE_SSB,                Hz(400)},     /*        SSB */
        {RIG_MODE_SSB,                Hz(200)},     /*        SSB */
        {RIG_MODE_AM,                 Hz(9000)},    /* Normal AM */
        {RIG_MODE_AM,                 Hz(6000)},    /* Narrow AM */
        {FT991_FM_RX_MODES,           Hz(16000)},   /* Normal FM */
        {FT991_FM_RX_MODES,           Hz(9000)},    /* Narrow FM */

        RIG_FLT_END,
    },

    .priv =               NULL,           /* private data FIXME: */

    .rig_init =           ft991_init,
    .rig_cleanup =        newcat_cleanup,
    .rig_open =           newcat_open,     /* port opened */
    .rig_close =          newcat_close,    /* port closed */

    .set_freq =           newcat_set_freq,
    .get_freq =           newcat_get_freq,
    .set_mode =           newcat_set_mode,
    .get_mode =           newcat_get_mode,
    .get_vfo =            newcat_get_vfo,
    .set_ptt =            newcat_set_ptt,
    .get_ptt =            newcat_get_ptt,
    .set_split_vfo =      ft991_set_split_vfo,
    .get_split_vfo =      ft991_get_split_vfo,
    .get_split_mode =     ft991_get_split_mode,
    .set_rit =            newcat_set_rit,
    .get_rit =            newcat_get_rit,
    .set_xit =            newcat_set_xit,
    .get_xit =            newcat_get_xit,
    .set_ant =            newcat_set_ant,
    .get_ant =            newcat_get_ant,
    .get_func =           newcat_get_func,
    .set_func =           newcat_set_func,
    .get_level =          newcat_get_level,
    .set_level =          newcat_set_level,
    .get_mem =            newcat_get_mem,
    .set_mem =            newcat_set_mem,
    .vfo_op =             newcat_vfo_op,
    .get_info =           newcat_get_info,
    .power2mW =           newcat_power2mW,
    .mW2power =           newcat_mW2power,
    .set_rptr_shift =     newcat_set_rptr_shift,
    .get_rptr_shift =     newcat_get_rptr_shift,
    .set_ctcss_tone =     newcat_set_ctcss_tone,
    .get_ctcss_tone =     newcat_get_ctcss_tone,
    .set_ctcss_sql  =     newcat_set_ctcss_sql,
    .get_ctcss_sql  =     newcat_get_ctcss_sql,
    .set_powerstat =      newcat_set_powerstat,
    .get_powerstat =      newcat_get_powerstat,
    .set_ts =             newcat_set_ts,
    .get_ts =             newcat_get_ts,
    .set_trn =            newcat_set_trn,
    .get_trn =            newcat_get_trn,
    .set_channel =        newcat_set_channel,
    .get_channel =        newcat_get_channel,

};

/*
 * rig_set_split_vfo*
 *
 * Set split operation for a given VFO
 *
 * Parameter     | Type   | Accepted/Expected Values
 * -------------------------------------------------------------------------
 *   RIG *      | input  | pointer to private data
 *   vfo        | input  | currVFO, VFOA, VFOB, MEM
 *   split      | input  | 0 = off, 1 = on
 *   tx_vfo     | input  | currVFO, VFOA, VFOB
 * -------------------------------------------------------------------------
 * Returns RIG_OK on success or an error code on failure
 *
 * Comments: Passing currVFO to vfo or tx_vfo will use the currently
 *           selected VFO obtained from the priv->current_vfo data structure.
 *           Only VFOA and VFOB are valid assignments for the tx_vfo.
 *           The tx_vfo is loaded first when assigning MEM to vfo to ensure
 *           the correct TX VFO is selected by the rig in split mode.
 *           An error is returned if vfo and tx_vfo are the same.
 */
int ft991_set_split_vfo(RIG *rig, vfo_t vfo, split_t split, vfo_t tx_vfo)
{
  struct newcat_priv_data *priv;
  struct rig_state *state;
  unsigned char ci;
  int err;

  rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

  if (!rig)
    return -RIG_EINVAL;

  rig_debug(RIG_DEBUG_TRACE, "%s: passed vfo = 0x%02x\n", __func__, vfo);
  rig_debug(RIG_DEBUG_TRACE, "%s: passed split = 0x%02x\n", __func__, split);
  rig_debug(RIG_DEBUG_TRACE, "%s: passed tx_vfo = 0x%02x\n", __func__, tx_vfo);

  priv = (struct newcat_priv_data *)rig->state.priv;
  state = &rig->state;

  // RX VFO and TX VFO cannot be the same, no support for MEM as TX VFO
  if (vfo == tx_vfo || tx_vfo == RIG_VFO_MEM)
    return -RIG_ENTARGET;

  switch(split) {
    case RIG_SPLIT_ON:
      ci = '3';
      break;
    case RIG_SPLIT_OFF:
      ci = '2';
      break;
    default:
      return -RIG_EINVAL;
  }

  snprintf(priv->cmd_str, sizeof(priv->cmd_str), "FT%c;", ci);
  if ( RIG_OK != (err = write_block(&state->rigport, priv->cmd_str, strlen(priv->cmd_str))))  {
    rig_debug(RIG_DEBUG_ERR, "%s: write_block err = %d\n", __func__, err);
    return err;
  }

  return RIG_OK;
}

/*
 * rig_get_split_vfo*
 *
 * Get split mode status for a given VFO
 *
 * Parameter     | Type   | Accepted/Expected Values
 * -------------------------------------------------------------------------
 *   RIG *      | input  | pointer to private data
 *   vfo        | input  | currVFO, Main, VFO, VFOA, VFOB, MEM
 *   split *    | output | 0 = on, 1 = off
 *   tx_vfo *   | output | VFOA, VFOB
 * -------------------------------------------------------------------------
 * Returns RIG_OK on success or an error code on failure
 *
 * Comments: The passed value for the vfo is ignored since can only split one way
 */
int ft991_get_split_vfo(RIG *rig, vfo_t vfo, split_t *split, vfo_t *tx_vfo)
{
  struct newcat_priv_data *priv;
  int err;

  rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

  if (!rig)
    return -RIG_EINVAL;

  rig_debug(RIG_DEBUG_TRACE, "%s: passed vfo = 0x%02x\n", __func__, vfo);

  priv = (struct newcat_priv_data *)rig->state.priv;

  snprintf(priv->cmd_str, sizeof(priv->cmd_str), "FT;");
  if (RIG_OK != (err = newcat_get_cmd (rig)))
    return err;

  // Get split mode status
  *split = priv->ret_data[2]=='1'; // 1=VFOB TX so is in split mode
  rig_debug(RIG_DEBUG_TRACE, "%s: set split = 0x%02x\n", __func__, *split);

  *tx_vfo = RIG_VFO_A;
  rig_debug(RIG_DEBUG_TRACE, "%s: set tx_vfo = 0x%02x\n", __func__, *tx_vfo);

  return RIG_OK;
}

/*
 * rig_get_split_mode*
 *
 * Get the '991 split TX mode
 *
 * Parameter    | Type      | Accepted/expected values
 * ------------------------------------------------------------------
 * *rig         | input     | pointer to private data
 * vfo          | input     | currVFO, VFOA, VFOB, MEM
 * *tx_mode     | output    | supported modes
 * *tx_width    | output    | supported widths
 * ------------------------------------------------------------------
 * Returns RIG_OK on success or an error code on failure
 *
 * Comments:    Checks to see if the 991 is in split mode, if so it
 *              checks which VFO is set for TX and then gets the
 *              mode and passband of that VFO and stores it into *tx_mode
 *              and tx_width respectively.  If not in split mode returns
 *              RIG_MODE_NONE and 0 Hz.
 *
 */

int ft991_get_split_mode(RIG *rig, vfo_t vfo, rmode_t *tx_mode, pbwidth_t *tx_width)
{
    int err;
    split_t split;
    vfo_t split_vfo;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (!rig)
        return -RIG_EINVAL;

    err = ft991_get_split_vfo(rig, vfo, &split, &split_vfo);
    if (err != RIG_OK)
        return err;

    switch ((int)split) {
    case TRUE:              /* '991 is in split mode */
        err = newcat_get_mode(rig, split_vfo, tx_mode, tx_width);
        if (err != RIG_OK)
            return err;
        break;
    default:
        *tx_mode = RIG_MODE_NONE;
        *tx_width = 0;
        break;
    }
    return RIG_OK;
}

int ft991_init(RIG *rig) {
  struct newcat_priv_data *priv;
  rig_debug(RIG_DEBUG_VERBOSE,"%s called, version %s\n", __func__,rig->caps->version);
  int ret = newcat_init(rig);
  if (ret != RIG_OK) return ret;
  priv = (struct newcat_priv_data *)rig->state.priv;
  priv->width_frequency = 9;
  priv->offset_rit = 13;
  rig->state.current_vfo = RIG_VFO_A;
  return RIG_OK;
}