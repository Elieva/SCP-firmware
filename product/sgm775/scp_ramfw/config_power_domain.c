/*
 * Arm SCP/MCP Software
 * Copyright (c) 2017-2018, Arm Limited and Contributors. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <stdint.h>
#include <string.h>
#include <fwk_element.h>
#include <fwk_macros.h>
#include <fwk_mm.h>
#include <fwk_module.h>
#include <fwk_module_idx.h>
#include <config_power_domain.h>
#include <config_ppu_v0.h>
#include <mod_system_power.h>
#include <mod_power_domain.h>
#include <mod_ppu_v1.h>
#include <sgm775_core.h>

static const char *core_pd_name_table[SGM775_CORE_PER_CLUSTER_MAX] = {
    "CLUS0CORE0", "CLUS0CORE1", "CLUS0CORE2", "CLUS0CORE3",
    "CLUS0CORE4", "CLUS0CORE5", "CLUS0CORE6", "CLUS0CORE7",
};

/* Mask of the allowed states for the systop power domain */
static const uint32_t systop_allowed_state_mask_table[] = {
    [0] = MOD_PD_STATE_OFF_MASK | MOD_PD_STATE_ON_MASK |
          (1 << MOD_SYSTEM_POWER_POWER_STATE_SLEEP0) |
          (1 << MOD_SYSTEM_POWER_POWER_STATE_SLEEP1)
};

/*
 * Mask of the allowed states for the top level power domains
 * (but the cluster power domains) depending on the system states.
 */
static const uint32_t toplevel_allowed_state_mask_table[] = {
    [MOD_PD_STATE_OFF] = MOD_PD_STATE_OFF_MASK,
    [MOD_PD_STATE_ON] = MOD_PD_STATE_OFF_MASK | MOD_PD_STATE_ON_MASK,
    [MOD_SYSTEM_POWER_POWER_STATE_SLEEP0] = MOD_PD_STATE_OFF_MASK,
    [MOD_SYSTEM_POWER_POWER_STATE_SLEEP1] = MOD_PD_STATE_OFF_MASK
};

/*
 * Mask of the allowed states for the cluster power domain depending on the
 * system states.
 */
static const uint32_t cluster_pd_allowed_state_mask_table[] = {
    [MOD_PD_STATE_OFF] = MOD_PD_STATE_OFF_MASK,
    [MOD_PD_STATE_ON] = MOD_PD_STATE_OFF_MASK | MOD_PD_STATE_ON_MASK |
        MOD_PD_STATE_SLEEP_MASK,
    [MOD_SYSTEM_POWER_POWER_STATE_SLEEP0] = MOD_PD_STATE_OFF_MASK,
    [MOD_SYSTEM_POWER_POWER_STATE_SLEEP1] = MOD_PD_STATE_OFF_MASK
};

/* Mask of the allowed states for a core depending on the cluster states. */
static const uint32_t core_pd_allowed_state_mask_table[] = {
    [MOD_PD_STATE_OFF] = MOD_PD_STATE_OFF_MASK,
    [MOD_PD_STATE_ON] = MOD_PD_STATE_OFF_MASK | MOD_PD_STATE_ON_MASK |
        MOD_PD_STATE_SLEEP_MASK,
    [MOD_PD_STATE_SLEEP] = MOD_PD_STATE_OFF_MASK | MOD_PD_STATE_SLEEP_MASK,
};

/* Power module specific configuration data (none) */
static const struct mod_power_domain_config sgm775_power_domain_config = { };

static struct fwk_element sgm775_power_domain_static_element_table[] = {
    [CONFIG_POWER_DOMAIN_SYSTOP_CHILD_CLUSTER0] = {
        .name = "CLUS0",
        .data = &((struct mod_power_domain_element_config) {
            .attributes.pd_type = MOD_PD_TYPE_CLUSTER,
            .tree_pos = MOD_PD_TREE_POS(
                MOD_PD_LEVEL_1,
                0,
                0,
                CONFIG_POWER_DOMAIN_SYSTOP_CHILD_CLUSTER0,
                0),
            .api_id = FWK_ID_API_INIT(FWK_MODULE_IDX_PPU_V1,
                                      MOD_PPU_V1_API_IDX_POWER_DOMAIN_DRIVER),
            .allowed_state_mask_table = cluster_pd_allowed_state_mask_table,
            .allowed_state_mask_table_size =
                FWK_ARRAY_SIZE(cluster_pd_allowed_state_mask_table)
        }),
    },
    [CONFIG_POWER_DOMAIN_SYSTOP_CHILD_DBGTOP] = {
        .name = "DBGTOP",
        .data = &((struct mod_power_domain_element_config) {
            .attributes.pd_type = MOD_PD_TYPE_DEVICE_DEBUG,
            .tree_pos = MOD_PD_TREE_POS(
                MOD_PD_LEVEL_1,
                0,
                0,
                CONFIG_POWER_DOMAIN_SYSTOP_CHILD_DBGTOP,
                0),
            .driver_id = FWK_ID_ELEMENT_INIT(
                FWK_MODULE_IDX_PPU_V0, PPU_V0_ELEMENT_IDX_DBGTOP),
            .api_id = FWK_ID_API_INIT(FWK_MODULE_IDX_PPU_V0, 0),
            .allowed_state_mask_table = toplevel_allowed_state_mask_table,
            .allowed_state_mask_table_size =
                FWK_ARRAY_SIZE(toplevel_allowed_state_mask_table)
        }),
    },
    [CONFIG_POWER_DOMAIN_SYSTOP_CHILD_DPU0TOP] = {
        .name = "DPU0TOP",
        .data = &((struct mod_power_domain_element_config) {
            .attributes.pd_type = MOD_PD_TYPE_DEVICE,
            .tree_pos = MOD_PD_TREE_POS(
                MOD_PD_LEVEL_1,
                0,
                0,
                CONFIG_POWER_DOMAIN_SYSTOP_CHILD_DPU0TOP,
                0),
            .driver_id = FWK_ID_ELEMENT_INIT(
                FWK_MODULE_IDX_PPU_V0, PPU_V0_ELEMENT_IDX_DPU0TOP),
            .api_id = FWK_ID_API_INIT(FWK_MODULE_IDX_PPU_V0, 0),
            .allowed_state_mask_table = toplevel_allowed_state_mask_table,
            .allowed_state_mask_table_size =
                FWK_ARRAY_SIZE(toplevel_allowed_state_mask_table)
        }),
    },
    [CONFIG_POWER_DOMAIN_SYSTOP_CHILD_DPU1TOP] = {
        .name = "DPU1TOP",
        .data = &((struct mod_power_domain_element_config) {
            .attributes.pd_type = MOD_PD_TYPE_DEVICE,
            .tree_pos = MOD_PD_TREE_POS(
                MOD_PD_LEVEL_1,
                0,
                0,
                CONFIG_POWER_DOMAIN_SYSTOP_CHILD_DPU1TOP,
                0),
            .driver_id = FWK_ID_ELEMENT_INIT(
                FWK_MODULE_IDX_PPU_V0, PPU_V0_ELEMENT_IDX_DPU1TOP),
            .api_id = FWK_ID_API_INIT(FWK_MODULE_IDX_PPU_V0, 0),
            .allowed_state_mask_table = toplevel_allowed_state_mask_table,
            .allowed_state_mask_table_size =
                FWK_ARRAY_SIZE(toplevel_allowed_state_mask_table)
        }),
    },
    [CONFIG_POWER_DOMAIN_SYSTOP_CHILD_GPUTOP] = {
        .name = "GPUTOP",
        .data = &((struct mod_power_domain_element_config) {
            .attributes.pd_type = MOD_PD_TYPE_DEVICE,
            .tree_pos = MOD_PD_TREE_POS(
                MOD_PD_LEVEL_1,
                0,
                0,
                CONFIG_POWER_DOMAIN_SYSTOP_CHILD_GPUTOP,
                0),
            .driver_id = FWK_ID_ELEMENT_INIT(
                FWK_MODULE_IDX_PPU_V0, PPU_V0_ELEMENT_IDX_GPUTOP),
            .api_id = FWK_ID_API_INIT(FWK_MODULE_IDX_PPU_V0, 0),
            .allowed_state_mask_table = toplevel_allowed_state_mask_table,
            .allowed_state_mask_table_size =
                FWK_ARRAY_SIZE(toplevel_allowed_state_mask_table)
        }),
    },
    [CONFIG_POWER_DOMAIN_SYSTOP_CHILD_VPUTOP] = {
        .name = "VPUTOP",
        .data = &((struct mod_power_domain_element_config) {
            .attributes.pd_type = MOD_PD_TYPE_DEVICE,
            .tree_pos = MOD_PD_TREE_POS(
                MOD_PD_LEVEL_1,
                0,
                0,
                CONFIG_POWER_DOMAIN_SYSTOP_CHILD_VPUTOP,
                0),
            .driver_id = FWK_ID_ELEMENT_INIT(
                FWK_MODULE_IDX_PPU_V0, PPU_V0_ELEMENT_IDX_VPUTOP),
            .api_id = FWK_ID_API_INIT(FWK_MODULE_IDX_PPU_V0, 0),
            .allowed_state_mask_table = toplevel_allowed_state_mask_table,
            .allowed_state_mask_table_size =
                FWK_ARRAY_SIZE(toplevel_allowed_state_mask_table)
        }),
    },
    [CONFIG_POWER_DOMAIN_SYSTOP_CHILD_COUNT] = {
        .name = "SYSTOP",
        .data = &((struct mod_power_domain_element_config) {
            .attributes.pd_type = MOD_PD_TYPE_SYSTEM,
            .tree_pos = MOD_PD_TREE_POS(
                MOD_PD_LEVEL_2, 0, 0, 0, 0),
            .driver_id = FWK_ID_MODULE_INIT(FWK_MODULE_IDX_SYSTEM_POWER),
            .api_id = FWK_ID_API_INIT(FWK_MODULE_IDX_SYSTEM_POWER,
                MOD_SYSTEM_POWER_API_IDX_PD_DRIVER),
            .allowed_state_mask_table = systop_allowed_state_mask_table,
            .allowed_state_mask_table_size =
                FWK_ARRAY_SIZE(systop_allowed_state_mask_table)
        }),
    },
};


/*
 * Function definitions with internal linkage
 */
static const struct fwk_element *sgm775_power_domain_get_element_table
    (fwk_id_t module_id)
{
    struct fwk_element *element_table, *element;
    struct mod_power_domain_element_config *pd_config_table, *pd_config;
    unsigned int core_idx;

    element_table = fwk_mm_calloc(
        sgm775_core_get_count()
        + FWK_ARRAY_SIZE(sgm775_power_domain_static_element_table)
        + 1, /* Terminator */
        sizeof(struct fwk_element));
    if (element_table == NULL)
        return NULL;

    pd_config_table = fwk_mm_calloc(sgm775_core_get_count(),
        sizeof(struct mod_power_domain_element_config));
    if (pd_config_table == NULL)
        return NULL;

    for (core_idx = 0; core_idx < sgm775_core_get_count(); core_idx++) {
        element = &element_table[core_idx];
        pd_config = &pd_config_table[core_idx];

        element->name = core_pd_name_table[core_idx];
        element->data = pd_config;

        pd_config->attributes.pd_type = MOD_PD_TYPE_CORE,
        pd_config->tree_pos = MOD_PD_TREE_POS(
            MOD_PD_LEVEL_0,
            0,
            0,
            CONFIG_POWER_DOMAIN_SYSTOP_CHILD_CLUSTER0,
            core_idx),
        pd_config->driver_id = FWK_ID_ELEMENT(FWK_MODULE_IDX_PPU_V1, core_idx),
        pd_config->api_id = FWK_ID_API(
            FWK_MODULE_IDX_PPU_V1, MOD_PPU_V1_API_IDX_POWER_DOMAIN_DRIVER),
        pd_config->allowed_state_mask_table = core_pd_allowed_state_mask_table,
        pd_config->allowed_state_mask_table_size =
            FWK_ARRAY_SIZE(core_pd_allowed_state_mask_table);
    }

    pd_config = (struct mod_power_domain_element_config *)
                    sgm775_power_domain_static_element_table
                        [CONFIG_POWER_DOMAIN_SYSTOP_CHILD_CLUSTER0]
                            .data;
    pd_config->driver_id =
        FWK_ID_ELEMENT(FWK_MODULE_IDX_PPU_V1, sgm775_core_get_count());

    memcpy(element_table + sgm775_core_get_count(),
           sgm775_power_domain_static_element_table,
           sizeof(sgm775_power_domain_static_element_table));

    return element_table;
}

/*
 * Power module configuration data
 */
struct fwk_module_config config_power_domain = {
    .get_element_table = sgm775_power_domain_get_element_table,
    .data = &sgm775_power_domain_config,
};
