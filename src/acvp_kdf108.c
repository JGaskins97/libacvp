/*****************************************************************************
* Copyright (c) 2017, Cisco Systems, Inc.
* All rights reserved.

* Redistribution and use in source and binary forms, with or without modification,
* are permitted provided that the following conditions are met:
*
* 1. Redistributions of source code must retain the above copyright notice,
*    this list of conditions and the following disclaimer.
*
* 2. Redistributions in binary form must reproduce the above copyright notice,
*    this list of conditions and the following disclaimer in the documentation
*    and/or other materials provided with the distribution.
*
* THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
* AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
* IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
* DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
* FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
* DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
* SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
* CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
* OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE
* USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*****************************************************************************/
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "acvp.h"
#include "acvp_lcl.h"
#include "parson.h"

/*
 * After the test case has been processed by the DUT, the results
 * need to be JSON formated to be included in the vector set results
 * file that will be uploaded to the server.  This routine handles
 * the JSON processing for a single test case.
 */
static ACVP_RESULT acvp_kdf108_output_tc (ACVP_CTX *ctx, ACVP_KDF108_TC *stc, JSON_Object *tc_rsp) {
    ACVP_RESULT rv;
    
    json_object_set_string(tc_rsp, "keyOut", stc->key_out);
    json_object_set_string(tc_rsp, "fixedData", stc->fixed_data);
    
    return ACVP_SUCCESS;
}

static ACVP_RESULT acvp_kdf108_init_tc (ACVP_CTX *ctx,
                                        ACVP_KDF108_TC *stc,
                                        unsigned int tc_id,
                                        char *kdf_mode,
                                        char *mac_mode,
                                        char *counter_location,
                                        int key_out_len,
                                        int counter_len,
                                        char *key_in,
                                        int deferred
) {
    memset(stc, 0x0, sizeof(ACVP_KDF108_TC));
    
    stc->mode = calloc(strlen(kdf_mode), sizeof(char));
    if (!stc->mode) { return ACVP_MALLOC_FAIL; }

    stc->mac_mode = calloc(strlen(mac_mode), sizeof(char));
    if (!stc->mac_mode) { return ACVP_MALLOC_FAIL; }

    stc->counter_location = calloc(strlen(counter_location), sizeof(char));
    if (!stc->counter_location) { return ACVP_MALLOC_FAIL; }

    stc->key_in = calloc(strlen(key_in), sizeof(char));
    if (!stc->key_in) { return ACVP_MALLOC_FAIL; }
    
    stc->tc_id = tc_id;
    stc->cipher = ACVP_KDF108;
    stc->mode = kdf_mode;
    stc->mac_mode = mac_mode;
    stc->counter_location = counter_location;
    stc->key_out_len = key_out_len;
    stc->counter_len = counter_len;
    stc->key_in = (unsigned char *)key_in;
    stc->deferred = deferred;
    
    return ACVP_SUCCESS;
}

/*
 * This function simply releases the data associated with
 * a test case.
 */
static ACVP_RESULT acvp_kdf108_release_tc (ACVP_KDF108_TC *stc) {
    free(stc->mode);
    free(stc->mac_mode);
    free(stc->counter_location);
    free(stc->key_in);
    free(stc->key_out);
    free(stc->fixed_data);
    
    memset(stc, 0x0, sizeof(ACVP_KDF108_TC));
    return ACVP_SUCCESS;
}

ACVP_RESULT acvp_kdf108_kat_handler (ACVP_CTX *ctx, JSON_Object *obj) {
    unsigned int tc_id;
    JSON_Value *groupval;
    JSON_Object *groupobj = NULL;
    JSON_Value *testval;
    JSON_Object *testobj = NULL;
    JSON_Array *groups;
    JSON_Array *tests;

    JSON_Value *reg_arry_val = NULL;
    JSON_Object *reg_obj = NULL;
    JSON_Array *reg_arry = NULL;

    int i, g_cnt;
    int j, t_cnt;

    JSON_Value *r_vs_val = NULL;
    JSON_Object *r_vs = NULL;
    JSON_Array *r_tarr = NULL; /* Response testarray */
    JSON_Value *r_tval = NULL; /* Response testval */
    JSON_Object *r_tobj = NULL; /* Response testobj */
    ACVP_CAPS_LIST *cap;
    ACVP_KDF108_TC stc;
    ACVP_TEST_CASE tc;
    ACVP_RESULT rv;
    const char *alg_str = "KDF";
    ACVP_CIPHER alg_id = ACVP_KDF108;
    int key_out_len, counter_len, deferred;
    char *kdf_mode = NULL, *mac_mode = NULL, *key_in = NULL, *counter_location = NULL;
    char *json_result;

    /*
     * Get a reference to the abstracted test case
     */
    tc.tc.kdf108 = &stc;

    /*
     * Get the crypto module handler for this hash algorithm
     */
    cap = acvp_locate_cap_entry(ctx, alg_id);
    if (!cap) {
        ACVP_LOG_ERR("ACVP server requesting unsupported capability");
        return (ACVP_UNSUPPORTED_OP);
    }

    /*
     * Create ACVP array for response
     */
    rv = acvp_create_array(&reg_obj, &reg_arry_val, &reg_arry);
    if (rv != ACVP_SUCCESS) {
        ACVP_LOG_ERR("Failed to create JSON response struct. ");
        return (rv);
    }

    /*
     * Start to build the JSON response
     */
    if (ctx->kat_resp) {
        json_value_free(ctx->kat_resp);
    }
    ctx->kat_resp = reg_arry_val;
    r_vs_val = json_value_init_object();
    r_vs = json_value_get_object(r_vs_val);

    json_object_set_number(r_vs, "vsId", ctx->vs_id);
    json_object_set_string(r_vs, "algorithm", alg_str);
    json_object_set_value(r_vs, "testResults", json_value_init_array());
    r_tarr = json_object_get_array(r_vs, "testResults");

    groups = json_object_get_array(obj, "testGroups");
    g_cnt = json_array_get_count(groups);
    for (i = 0; i < g_cnt; i++) {
        groupval = json_array_get_value(groups, i);
        groupobj = json_value_get_object(groupval);

        ACVP_LOG_INFO("    Test group: %d", i);
        
        kdf_mode = json_object_get_string(groupobj, "kdfMode");
        mac_mode = json_object_get_string(groupobj, "macMode");
        key_out_len = json_object_get_number(groupobj, "keyOutLength");
        counter_len = json_object_get_number(groupobj, "counterLength");
        counter_location = json_object_get_string(groupobj, "counterLocation");

        tests = json_object_get_array(groupobj, "tests");
        t_cnt = json_array_get_count(tests);
        for (j = 0; j < t_cnt; j++) {
            ACVP_LOG_INFO("Found new kdf108 test vector...");
            testval = json_array_get_value(tests, j);
            testobj = json_value_get_object(testval);

            tc_id = (unsigned int) json_object_get_number(testobj, "tcId");
            key_in = json_object_get_string(testobj, "keyIn");
            deferred = json_object_get_boolean(testobj, "deferred");

            ACVP_LOG_INFO("        Test case: %d", j);
            ACVP_LOG_INFO("             tcId: %d", tc_id);
            ACVP_LOG_INFO("          macMode: %d", mac_mode);
            ACVP_LOG_INFO("            keyIn: %d", key_in);

            /*
             * Create a new test case in the response
             */
            r_tval = json_value_init_object();
            r_tobj = json_value_get_object(r_tval);

            json_object_set_number(r_tobj, "tcId", tc_id);

            /*
             * Setup the test case data that will be passed down to
             * the crypto module.
             * TODO: this does mallocs, we can probably do the mallocs once for
             *       the entire vector set to be more efficient
             */
            acvp_kdf108_init_tc(ctx, &stc, tc_id, kdf_mode, mac_mode,
                                counter_location, key_out_len,
                                counter_len, key_in, deferred);

            /* Process the current test vector... */
            rv = (cap->crypto_handler)(&tc);
            if (rv != ACVP_SUCCESS) {
                ACVP_LOG_ERR("crypto module failed the operation");
                return ACVP_CRYPTO_MODULE_FAIL;
            }

            /*
             * Output the test case results using JSON
            */
            rv = acvp_kdf108_output_tc(ctx, &stc, r_tobj);
            if (rv != ACVP_SUCCESS) {
                ACVP_LOG_ERR("JSON output failure in kdf135 tpm module");
                return rv;
            }
            /*
             * Release all the memory associated with the test case
             */
            acvp_kdf108_release_tc(&stc);

            /* Append the test response value to array */
            json_array_append_value(r_tarr, r_tval);
        }
    }

    json_array_append_value(reg_arry, r_vs_val);

    json_result = json_serialize_to_string_pretty(ctx->kat_resp);
    if (ctx->debug == ACVP_LOG_LVL_VERBOSE) {
        printf("\n\n%s\n\n", json_result);
    } else {
        ACVP_LOG_INFO("\n\n%s\n\n", json_result);
    }
    json_free_serialized_string(json_result);

    return ACVP_SUCCESS;
}
