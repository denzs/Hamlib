/*
 *  Hamlib Barrett backend - main file
 *  Copyright (c) 2017 by Michael Black W9MDB
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <math.h>

#include <hamlib/rig.h>
#include <serial.h>
#include <misc.h>
#include <cal.h>
#include <token.h>
#include <register.h>

#include "barrett.h"

#define MAXCMDLEN 32

#define BARRETT_VFOS (RIG_VFO_A|RIG_VFO_MEM)

#define BARRETT_MODES (RIG_MODE_AM | RIG_MODE_CW | RIG_MODE_RTTY | RIG_MODE_SSB)

#define BARRETT_LEVELS (RIG_LEVEL_STRENGTH)


static int barrett_init(RIG *rig);
static int barrett_cleanup(RIG *rig);
static int barrett_set_freq(RIG *rig, vfo_t vfo, freq_t freq);
static int barrett_get_freq(RIG *rig, vfo_t vfo, freq_t *freq);
static int barrett_set_ptt(RIG *rig, vfo_t vfo, ptt_t ptt);
static int barrett_get_ptt(RIG *rig, vfo_t vfo, ptt_t *ptt);
static int barrett_set_mode(RIG *rig, vfo_t vfo, rmode_t mode, pbwidth_t width);
static int barrett_get_mode(RIG *rig, vfo_t vfo, rmode_t *mode, pbwidth_t *width);
static int barrett_set_split_freq(RIG *rig, vfo_t vfo, freq_t tx_freq);
static int barrett_set_split_vfo(RIG *rig, vfo_t rxvfo, split_t split, vfo_t txvfo);
static int barrett_get_split_vfo(RIG *rig, vfo_t vfo, split_t *split, vfo_t *txvfo);
static int barrett_get_level(RIG *rig, vfo_t vfo, setting_t level, value_t *val);
static const char *barrett_get_info(RIG *rig);


const struct rig_caps barrett_caps = {
    .rig_model = RIG_MODEL_BARRETT_2050,
    .model_name = "2050",
    .mfg_name = "Barrett",
    .version = BACKEND_VER,
    .copyright = "LGPL",
    .status = RIG_STATUS_BETA,
    .rig_type = RIG_TYPE_TRANSCEIVER,
    .targetable_vfo = RIG_TARGETABLE_FREQ | RIG_TARGETABLE_MODE,
    .ptt_type = RIG_PTT_RIG,
    .dcd_type = RIG_DCD_NONE,
    .port_type = RIG_PORT_SERIAL,
    .serial_rate_min = 9600,
    .serial_rate_max = 9600,
    .serial_data_bits = 8,
    .serial_stop_bits = 1,
    .serial_parity = RIG_PARITY_NONE,
    .serial_handshake = RIG_HANDSHAKE_XONXOFF,
    .write_delay = 0,
    .post_write_delay = 50,
    .timeout = 1000,
    .retry = 3,

    .has_get_func = RIG_FUNC_NONE,
    .has_set_func = RIG_FUNC_NONE,
    .has_get_level = BARRETT_LEVELS,
    .has_set_level = RIG_LEVEL_NONE,
    .has_get_parm = RIG_PARM_NONE,
    .has_set_parm = RIG_PARM_NONE,
//  .level_gran =          { [LVL_CWPITCH] = { .step = { .i = 10 } } },
//  .ctcss_list =          common_ctcss_list,
//  .dcs_list =            full_dcs_list,
//  2050 does have channels...not implemented yet as no need yet
//  .chan_list =   {
//                        {   0,  18, RIG_MTYPE_MEM, DUMMY_MEM_CAP },
//                        {  19,  19, RIG_MTYPE_CALL },
//                        {  20,  NB_CHAN-1, RIG_MTYPE_EDGE },
//                        RIG_CHAN_END,
//                 },
// .scan_ops =    DUMMY_SCAN,
// .vfo_ops =     DUMMY_VFO_OP,
    .transceive = RIG_TRN_RIG,
    .rx_range_list1 = {{
            .start = kHz(1600), .end = MHz(30), .modes = BARRETT_MODES,
            .low_power = -1, .high_power = -1, BARRETT_VFOS, RIG_ANT_1
        },
        RIG_FRNG_END,
    },
    .rx_range_list1 = {RIG_FRNG_END,},
    .tx_range_list1 = {RIG_FRNG_END,},
    .tx_range_list2 = {RIG_FRNG_END,},
    .tuning_steps =  { {BARRETT_MODES, 1}, {BARRETT_MODES, RIG_TS_ANY}, RIG_TS_END, },
    .filters = {
        {RIG_MODE_SSB | RIG_MODE_CW | RIG_MODE_RTTY, kHz(2.4)},
        {RIG_MODE_CW, Hz(500)},
        {RIG_MODE_AM, kHz(8)},
        {RIG_MODE_AM, kHz(2.4)},
        RIG_FLT_END,
    },
    .priv = NULL,

//  .extlevels =    dummy_ext_levels,
//  .extparms =     dummy_ext_parms,
//  .cfgparams =    dummy_cfg_params,

    .rig_init = barrett_init,
    .rig_cleanup =  barrett_cleanup,

//  .set_conf =     dummy_set_conf,
//  .get_conf =     dummy_get_conf,

    .set_freq = barrett_set_freq,
    .get_freq = barrett_get_freq,
    .set_mode = barrett_set_mode,
    .get_mode = barrett_get_mode,

//  .set_powerstat =  dummy_set_powerstat,
//  .get_powerstat =  dummy_get_powerstat,
//  .set_level =     dummy_set_level,
    .get_level =     barrett_get_level,
//  .set_func =      dummy_set_func,
//  .get_func =      dummy_get_func,
//  .set_parm =      dummy_set_parm,
//  .get_parm =      dummy_get_parm,
//  .set_ext_level = dummy_set_ext_level,
//  .get_ext_level = dummy_get_ext_level,
//  .set_ext_parm =  dummy_set_ext_parm,
//  .get_ext_parm =  dummy_get_ext_parm,

    .get_info =      barrett_get_info,
    .set_ptt = barrett_set_ptt,
    .get_ptt = barrett_get_ptt,
//  .get_dcd =    dummy_get_dcd,
//  .set_rptr_shift =     dummy_set_rptr_shift,
//  .get_rptr_shift =     dummy_get_rptr_shift,
//  .set_rptr_offs =      dummy_set_rptr_offs,
//  .get_rptr_offs =      dummy_get_rptr_offs,
//  .set_ctcss_tone =     dummy_set_ctcss_tone,
//  .get_ctcss_tone =     dummy_get_ctcss_tone,
//  .set_dcs_code =       dummy_set_dcs_code,
//  .get_dcs_code =       dummy_get_dcs_code,
//  .set_ctcss_sql =      dummy_set_ctcss_sql,
//  .get_ctcss_sql =      dummy_get_ctcss_sql,
//  .set_dcs_sql =        dummy_set_dcs_sql,
//  .get_dcs_sql =        dummy_get_dcs_sql,
    .set_split_freq = barrett_set_split_freq,
//  .get_split_freq =     dummy_get_split_freq,
//  .set_split_mode =     dummy_set_split_mode,
//  .get_split_mode =     dummy_get_split_mode,
    .set_split_vfo =      barrett_set_split_vfo,
    .get_split_vfo =      barrett_get_split_vfo,
//  .set_rit =    dummy_set_rit,
//  .get_rit =    dummy_get_rit,
//  .set_xit =    dummy_set_xit,
//  .get_xit =    dummy_get_xit,
//  .set_ts =     dummy_set_ts,
//  .get_ts =     dummy_get_ts,
//  .set_ant =    dummy_set_ant,
//  .get_ant =    dummy_get_ant,
//  .set_bank =   dummy_set_bank,
//  .set_mem =    dummy_set_mem,
//  .get_mem =    dummy_get_mem,
//  .vfo_op =     dummy_vfo_op,
//  .scan =               dummy_scan,
//  .send_dtmf =  dummy_send_dtmf,
//  .recv_dtmf =  dummy_recv_dtmf,
//  .send_morse =  dummy_send_morse,
//  .set_channel =        dummy_set_channel,
//  .get_channel =        dummy_get_channel,
//  .set_trn =    dummy_set_trn,
//  .get_trn =    dummy_get_trn,
//  .power2mW =   dummy_power2mW,
//  .mW2power =   dummy_mW2power,
};


DECLARE_INITRIG_BACKEND(barrett)
{
    rig_debug(RIG_DEBUG_VERBOSE, "barrett: _init called\n");

    rig_register(&barrett_caps);
    rig_debug(RIG_DEBUG_VERBOSE, "barrett: _init back from rig_register\n");

    return RIG_OK;
}


int barrett_transaction(RIG *rig, char *cmd, int expected, char **result)
{
    char cmd_buf[MAXCMDLEN];
    int retval, cmd_len;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: cmd=%s\n", __FUNCTION__, cmd);
    struct rig_state *rs = &rig->state;
    struct barrett_priv_data *priv = rig->state.priv;

    cmd_len = snprintf(cmd_buf, sizeof(cmd_buf), "%s%s", cmd, EOM);

    serial_flush(&rs->rigport);
    retval = write_block(&rs->rigport, cmd_buf, cmd_len);

    if (retval < 0) {
        return retval;
    }

    if (expected == 0) {
        // response format is 0x11,data...,0x0d,0x0a,0x13
        retval = read_string(&rs->rigport, priv->ret_data, sizeof(priv->ret_data), "\x11", 1);
        rig_debug(RIG_DEBUG_VERBOSE, "%s: resultlen=%d\n", __FUNCTION__, strlen(priv->ret_data));

        if (retval < 0) {
            return retval;
        }
    } else {
        retval = read_block(&rs->rigport, priv->ret_data, expected);

        if (retval < 0) {
            return retval;
        }
    }

    rig_debug(RIG_DEBUG_VERBOSE, "%s: retval=%d\n", __FUNCTION__, retval);
    dump_hex((const unsigned char *)priv->ret_data, strlen(priv->ret_data));
    char *p = priv->ret_data;
    char xon = p[0];
    char xoff = p[strlen(p) - 1];

    if (xon == 0x13 && xoff == 0x11) {
        rig_debug(RIG_DEBUG_ERR, "%s: removing xoff char\n", __FUNCTION__);
        p[strlen(p) - 1] = 0;
    } else {
        rig_debug(RIG_DEBUG_ERR, "%s: expected XOFF=0x13 as first and XON=0x11 as last byte, got %02x/%02x\n", __FUNCTION__, xon, xoff);
    }

    rig_debug(RIG_DEBUG_ERR, "%s: removing xon char\n", __FUNCTION__);
    // Remove the XON char if there
    p = memchr(priv->ret_data, 0x11, strlen(priv->ret_data));

    if (p) { *p = 0; }



    rig_debug(RIG_DEBUG_VERBOSE, "%s: result = %04x\n", __FUNCTION__, result);

    if (result != NULL) {
        rig_debug(RIG_DEBUG_VERBOSE, "%s: setting result\n", __FUNCTION__);
        if (priv->ret_data[0] == 0x13) { // we'll return from the 1st good char
            *result = &(priv->ret_data[1]);
        }
        else { // some commands like IAL don't give XOFF but XON is there -- is this a bug?
            *result = &(priv->ret_data[0]);
        }
        // See how many CR's we have
        int n = 0;

        for (p = *result; *p; ++p) {
            if (*p == 0x0d) { ++n; }
        }

        // if only 1 CR then we'll truncate string
        // Several commands can return multiline strings and we'll leave them alone
        if (n == 1) { strtok(*result, "\r"); }

        dump_hex((const unsigned char *)*result, strlen(*result));
        rig_debug(RIG_DEBUG_VERBOSE, "%s: returning result=%s\n", __FUNCTION__, *result);
    } else {
        rig_debug(RIG_DEBUG_VERBOSE, "%s: no result requested=%s\n", __FUNCTION__);
    }

    return RIG_OK;
}

int barrett_init(RIG *rig)
{
    struct barrett_priv_data *priv;
    rig_debug(RIG_DEBUG_VERBOSE, "%s version %s\n", __FUNCTION__, rig->caps->version);

    if (!rig || !rig->caps) {
        return -RIG_EINVAL;
    }

    priv = (struct barrett_priv_data *)calloc(1, sizeof(struct barrett_priv_data));

    if (!priv) {
        return -RIG_ENOMEM;
    }

    rig->state.priv = (void *)priv;

    return RIG_OK;
}

/*
 * barrett_cleanup
 *
 */

int barrett_cleanup(RIG *rig)
{

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (!rig) {
        return -RIG_EINVAL;
    }

    if (rig->state.priv) {
        free(rig->state.priv);
    }

    rig->state.priv = NULL;

    return RIG_OK;
}


/*
 * barrett_get_freq
 * Assumes rig!=NULL, rig->state.priv!=NULL, freq!=NULL
 */
int barrett_get_freq(RIG *rig, vfo_t vfo, freq_t *freq)
{
    int retval;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: vfo=%s\n", __FUNCTION__, rig_strvfo(vfo));
    *freq = 0;

    char *response = NULL;

    if (vfo == RIG_VFO_B) { // We treat the TX VFO as VFO_B and RX VFO as VFO_A
        retval = barrett_transaction(rig, "IT", 0, &response);
    } else {
        retval = barrett_transaction(rig, "IR", 0, &response);
    }

    if (retval != RIG_OK) {
        rig_debug(RIG_DEBUG_ERR, "%s: invalid response=\n", __FUNCTION__, response);
        return retval;
    }

    retval = sscanf(response, "%lg", freq);

    if (retval != 1) {
        rig_debug(RIG_DEBUG_ERR, "Unable to parse response\n");
        return -RIG_EPROTO;
    }

    return RIG_OK;
}

/*
 * barrett_set_freq
 * assumes rig!=NULL, rig->state.priv!=NULL
 */
int barrett_set_freq(RIG *rig, vfo_t vfo, freq_t freq)
{
    char cmd_buf[MAXCMDLEN];
    int retval;
    struct barrett_priv_data *priv = rig->state.priv;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: vfo=%s freq=%.0f\n", __FUNCTION__, rig_strvfo(vfo), freq);

    // If we are not explicity asking for VFO_B then we'll set the receive side also
    if (vfo != RIG_VFO_B) {
        sprintf((char *) cmd_buf, "PR%08.0f", freq);
        char *response = NULL;
        retval = barrett_transaction(rig, cmd_buf, 0, &response);

        if (retval < 0) {
            return retval;
        }

        //dump_hex((unsigned char *)response, strlen(response));

        if (strncmp(response, "OK", 2) != 0) {
            rig_debug(RIG_DEBUG_ERR, "%s: Expected OK, got '%s'\n", __FUNCTION__, response);
            return -RIG_EINVAL;
        }
    }

    if (priv->split == 0 || vfo == RIG_VFO_B) { // if we aren't in split mode we have to set the TX VFO too
        sprintf((char *) cmd_buf, "PT%08.0f", freq);
        char *response = NULL;
        retval = barrett_transaction(rig, cmd_buf, 0, &response);

        if (retval < 0) {
            return retval;
        }

        if (strncmp(response, "OK", 2) != 0) {
            rig_debug(RIG_DEBUG_ERR, "%s: Expected OK, got '%s'\n", __FUNCTION__, response);
            return -RIG_EINVAL;
        }
    }

    return RIG_OK;
}


/*
 * barrett_set_ptt
 * Assumes rig!=NULL
 */
int barrett_set_ptt(RIG *rig, vfo_t vfo, ptt_t ptt)
{
    int retval;
    char cmd_buf[MAXCMDLEN];

    rig_debug(RIG_DEBUG_VERBOSE, "%s: ptt=%d\n", __FUNCTION__, ptt);

    // we need a little extra time before we assert PTT
    // testing with rigctld worked, but from WSJT-X did not
    // WSJT-X is just a little faster without the network timing
    usleep(100 * 1000);
    sprintf(cmd_buf, "XP%d", ptt);
    char *response = NULL;
    retval = barrett_transaction(rig, cmd_buf, 0, &response);

    if (retval < 0) {
        rig_debug(RIG_DEBUG_ERR, "%s: invalid response=\n", __FUNCTION__, response);
        return retval;
    }

    if (strncmp(response, "OK", 2) != 0) {
        rig_debug(RIG_DEBUG_ERR, "%s: Expected OK, got '%s'\n", __FUNCTION__, response);
        return -RIG_EINVAL;
    }

    rig_debug(RIG_DEBUG_VERBOSE, "%s: cmd:IP result=%s\n", __FUNCTION__, response);

    return RIG_OK;
}

/*
 * barrett_get_ptt
 * Assumes rig!=NULL
 */
int barrett_get_ptt(RIG *rig, vfo_t vfo, ptt_t *ptt)
{
    int retval;
    char *response = NULL;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: vfo=%s\n", __FUNCTION__, rig_strvfo(vfo));

    retval = barrett_transaction(rig, "IP", 0, &response);

    if (retval != RIG_OK) {
        rig_debug(RIG_DEBUG_ERR, "%s: error response?='%s'\n", response);
        return retval;
    }

    char c = response[0];

    if (c == '1' || c == '0') {
        *ptt = c - '0';
    } else {
        rig_debug(RIG_DEBUG_ERR, "%s: error response='%s'\n", response);
        return -RIG_EPROTO;
    }

    return RIG_OK;
}

/*
 * barrett_set_mode
 * Assumes rig!=NULL
 * Note that 2050 does not have set or get width
 */
int barrett_set_mode(RIG *rig, vfo_t vfo, rmode_t mode, pbwidth_t width)
{
    char cmd_buf[32], ttmode;
    int retval;

    //struct tt588_priv_data *priv = (struct tt588_priv_data *) rig->state.priv;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: vfo=%s mode=%d width=%d\n", __FUNCTION__, rig_strvfo(vfo), mode, width);

    switch (mode) {
    case RIG_MODE_USB:
        ttmode = 'U';
        break;

    case RIG_MODE_LSB:
        ttmode = 'L';
        break;

    case RIG_MODE_CW:
        ttmode = 'C';
        break;

    case RIG_MODE_AM:
        ttmode = 'A';
        break;

    case RIG_MODE_RTTY:
        ttmode = 'F';
        break;

    default:
        rig_debug(RIG_DEBUG_ERR, "%s: unsupported mode %d\n", __FUNCTION__, mode);
        return -RIG_EINVAL;
    }

    sprintf((char *) cmd_buf, "XB%c" EOM, ttmode);

    retval = barrett_transaction(rig, cmd_buf, 0, NULL);

    if (retval < 0) {
        return retval;
    }

    return RIG_OK;
}

/*
 * barrett_get_mode
 * Assumes rig!=NULL
 * Note that 2050 does not have set or get width
 */
int barrett_get_mode(RIG *rig, vfo_t vfo, rmode_t *mode, pbwidth_t *width)
{
    rig_debug(RIG_DEBUG_VERBOSE, "%s: vfo=%s\n", __FUNCTION__, rig_strvfo(vfo));

    char *result = NULL;
    int retval = barrett_transaction(rig, "IB", 0, &result);

    if (retval != RIG_OK) {
        rig_debug(RIG_DEBUG_ERR, "%s: bad response=%s\n", __FUNCTION__, result);
        return retval;
    }

    //dump_hex((unsigned char *)result,strlen(result));
    switch (result[1]) {
    case 'L':
        *mode = RIG_MODE_LSB;
        break;

    case 'U':
        *mode = RIG_MODE_USB;
        break;

    case 'A':
        *mode = RIG_MODE_AM;
        break;

    case 'F':
        *mode = RIG_MODE_RTTY;
        break;

    case 'C':
        *mode = RIG_MODE_CW;
        break;

    default:
        rig_debug(RIG_DEBUG_ERR, "%s: Unknown mode='%c%c'\n", __FUNCTION__,  result[0], result[1]);
        return -RIG_EPROTO;
    }

    *width = 3000; // we'll default this to 3000 for now
    rig_debug(RIG_DEBUG_VERBOSE, "%s: vfo=%s mode=%s width=%d\n", __FUNCTION__, rig_strvfo(vfo), rig_strrmode(*mode), *width);
    return RIG_OK;
}

#if 0
int barrett_get_vfo(RIG *rig, vfo_t *vfo)
{
    *vfo = RIG_VFO_A;

    if (check_vfo(*vfo) == FALSE) {
        rig_debug(RIG_DEBUG_ERR, "%s: unsupported VFO %s\n", __FUNCTION__, rig_strvfo(*vfo));
        return -RIG_EINVAL;
    }

    rig_debug(RIG_DEBUG_VERBOSE, "%s: vfo=%s\n", __FUNCTION__, rig_strvfo(*vfo));
    return RIG_OK;
}
#endif

/*
 * barrett_set_split_freq
 */
int barrett_set_split_freq(RIG *rig, vfo_t vfo, freq_t tx_freq)
{
    // The 2050 only has one RX and one TX VFO -- it's not treated as VFOA/VFOB
    char cmd_buf[MAXCMDLEN];

    rig_debug(RIG_DEBUG_VERBOSE, "%s: vfo=%s freq=%g\n", __FUNCTION__, rig_strvfo(vfo), tx_freq);

    sprintf((char *) cmd_buf, "PT%08.0f" EOM, tx_freq);

    int retval = barrett_transaction(rig, cmd_buf, 0, NULL);

    if (retval < 0) {
        return retval;
    }

    return RIG_OK;
}

int barrett_set_split_vfo(RIG *rig, vfo_t rxvfo, split_t split, vfo_t txvfo)
{
    struct barrett_priv_data *priv;

    priv = rig->state.priv;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called rxvfo=%s, txvfo=%s, split=%d\n", __FUNCTION__, rig_strvfo(rxvfo), rig_strvfo(txvfo), split);
    priv->split = split;
    return RIG_OK;
}

int barrett_get_split_vfo(RIG *rig, vfo_t rxvfo, split_t *split, vfo_t *txvfo)
{
    struct barrett_priv_data *priv;

    priv = rig->state.priv;

    *split = priv->split;
    *txvfo = RIG_VFO_B; // constant
    rig_debug(RIG_DEBUG_VERBOSE, "%s called rxvfo=%s, txvfo=%s, split=%d\n", __FUNCTION__, rig_strvfo(rxvfo), rig_strvfo(*txvfo), *split);
    return RIG_OK;
}

/*
 * barrett_get_level
 */
int barrett_get_level(RIG *rig, vfo_t vfo, setting_t level, value_t *val)
{
    int retval = 0;
    char *response = NULL;

    switch (level) {
    case RIG_LEVEL_STRENGTH:
        retval = barrett_transaction(rig, "IAL", 0, &response);

        if (retval < 0) {
            rig_debug(RIG_DEBUG_ERR, "%s: invalid response=%s\n", __FUNCTION__, level);
            return retval;
        }

        int strength;
        int n = sscanf(response, "%2d", &strength);
        if (n==1) {
            val->i = strength;
        }
        else {
            rig_debug(RIG_DEBUG_ERR, "%s: unable to parse STRENGHT from %s\n", __FUNCTION__, response);
            return -RIG_EPROTO;
        }
        break;

    default:
        rig_debug(RIG_DEBUG_ERR, "%s: unsupported level %d\n", __FUNCTION__, level);
        return -RIG_EINVAL;
    }

    rig_debug(RIG_DEBUG_VERBOSE, "%s: vfo=%s level=%d val=%d\n", __FUNCTION__, rig_strvfo(vfo), level, *val);

    return RIG_OK;
}

/*
 * barrett_get_info
 */
const char *barrett_get_info(RIG *rig)
{
    char *response = NULL;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __FUNCTION__);

    int retval = barrett_transaction(rig, "IVF", 0, &response);

    if (retval == RIG_OK) {
        rig_debug(RIG_DEBUG_ERR, "%s: result=%s\n", __FUNCTION__, response, strlen(response));
    } else {
        rig_debug(RIG_DEBUG_VERBOSE, "Software Version %s\n", response);
    }

    return response;
}