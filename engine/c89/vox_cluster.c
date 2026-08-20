/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "vox/vox_cluster.h"
#include "vox_kernel_private.h"

static int cluster_in_bounds(vox_u32 x, vox_u32 y, vox_u32 z)
{
    return x < VOX_WORLD_WIDTH && y < VOX_WORLD_HEIGHT &&
           z < VOX_WORLD_DEPTH;
}

static int cluster_contains(const vox_structure_cluster *cluster,
                            vox_u16 x, vox_u16 y, vox_u16 z)
{
    vox_u16 i;
    for (i = 0U; i < cluster->count; ++i) {
        if (cluster->cells[i].x == x && cluster->cells[i].y == y &&
            cluster->cells[i].z == z) return 1;
    }
    return 0;
}

static int cluster_structural(vox_u16 material)
{
    return material == VOX_MAT_STONE || material == VOX_MAT_SOIL ||
           material == VOX_MAT_COAL || material == VOX_MAT_BIOMASS ||
           material == VOX_MAT_METAL || material == VOX_MAT_SAND;
}

static int cluster_structural_cell(const vox_cell *cell)
{
    return cell != 0 &&
           (cell->flags & (VOX_CELL_FIXTURE | VOX_CELL_LOOSE)) == 0U &&
           cluster_structural(cell->material);
}

static int structure_bears_load(const vox_cell *cell)
{
    return cell != 0 && (cell->material == VOX_MAT_BEDROCK ||
                          cluster_structural_cell(cell));
}

static int structure_supported(const vox_world *world, vox_u32 x,
                               vox_u32 y, vox_u32 z)
{
    vox_u32 reach;
    vox_i32 offset;
    if (y + 1U >= VOX_WORLD_HEIGHT) return 1;
    for (reach = 0U; reach <= VOX_STRUCTURE_COHESION_CELLS; ++reach) {
        for (offset = -(vox_i32)reach; offset <= (vox_i32)reach; ++offset) {
            vox_i32 sample_x = (vox_i32)x + offset;
            const vox_cell *cell;
            if (sample_x < 0L || sample_x >= (vox_i32)VOX_WORLD_WIDTH) {
                continue;
            }
            cell = vox_world_cell(world, (vox_u32)sample_x, y + 1U, z);
            if (cell != 0 && (cell->flags & VOX_CELL_LOOSE) == 0U &&
                structure_bears_load(cell)) return 1;
        }
    }
    return 0;
}

static vox_u16 structure_chunk_index(vox_u32 chunk_x, vox_u32 chunk_y)
{
    return (vox_u16)(chunk_y * VOX_WORLD_CHUNKS_X + chunk_x);
}

static void structure_mark_chunk(vox_structure_state *state, vox_u16 index,
                                 vox_u16 source, vox_u16 weapon,
                                 vox_u16 arm_collapse)
{
    vox_structure_chunk_state *chunk = &state->chunks[index];
    if (arm_collapse != 0U) {
        chunk->collapse_armed = 1U;
        chunk->source = source;
        chunk->weapon = weapon;
    }
    if (chunk->dirty != 0U) return;
    if (state->frontier_count >= VOX_STRUCTURE_FRONTIER_CAPACITY) {
        state->discarded_frontier++;
        return;
    }
    state->frontier[(state->frontier_head + state->frontier_count) %
                    VOX_STRUCTURE_FRONTIER_CAPACITY] = index;
    state->frontier_count++;
    chunk->dirty = 1U;
}

static void structure_queue_collapse(vox_structure_state *state,
                                     vox_u16 chunk_index, vox_u16 x,
                                     vox_u16 y, vox_u16 z,
                                     vox_u16 risk_q8)
{
    vox_u16 slot;
    vox_structure_chunk_state *chunk = &state->chunks[chunk_index];
    if (chunk->collapse_pending != 0U) return;
    if (state->collapse_count >= VOX_STRUCTURE_COLLAPSE_CAPACITY) {
        state->discarded_collapse++;
        return;
    }
    slot = (vox_u16)((state->collapse_head + state->collapse_count) %
                     VOX_STRUCTURE_COLLAPSE_CAPACITY);
    state->collapses[slot].x = x;
    state->collapses[slot].y = y;
    state->collapses[slot].z = z;
    state->collapses[slot].chunk_index = chunk_index;
    state->collapses[slot].source = chunk->source;
    state->collapses[slot].weapon = chunk->weapon;
    state->collapses[slot].risk_q8 = risk_q8;
    state->collapses[slot].reserved = 0U;
    state->collapse_count++;
    chunk->collapse_pending = 1U;
}

static vox_u32 structure_hash_mix(vox_u32 hash, vox_u32 value)
{
    hash ^= value;
    hash *= 16777619U;
    return hash;
}

void vox_cluster_init(vox_structure_cluster *cluster)
{
    vox_u16 i;
    if (cluster == 0) return;
    cluster->count = 0U;
    cluster->complete = 0U;
    cluster->min_x = 65535U;
    cluster->max_x = 0U;
    cluster->min_y = 65535U;
    cluster->max_y = 0U;
    cluster->min_z = 65535U;
    cluster->max_z = 0U;
    cluster->anchored = 0U;
    cluster->support_q8 = 0U;
    cluster->load_q8 = 0U;
    for (i = 0U; i < VOX_CLUSTER_MAX_CELLS; ++i) {
        cluster->cells[i].x = 0U;
        cluster->cells[i].y = 0U;
        cluster->cells[i].z = 0U;
        cluster->cells[i].material = VOX_MAT_AIR;
    }
}

vox_result vox_cluster_extract(vox_world *world, vox_u32 x, vox_u32 y,
                               vox_u32 z, vox_structure_cluster *cluster)
{
    vox_u16 cursor = 0U;
    vox_u16 overflow = 0U;
    if (world == 0 || cluster == 0 || world->abi_version != VOX_ABI_VERSION ||
        !cluster_in_bounds(x, y, z)) return VOX_ERR_INVALID;
    vox_cluster_init(cluster);
    if (vox_world_cell(world, x, y, z) == 0 ||
        !cluster_structural_cell(vox_world_cell(world, x, y, z))) {
        cluster->complete = 1U;
        return VOX_OK;
    }
    cluster->cells[0].x = (vox_u16)x;
    cluster->cells[0].y = (vox_u16)y;
    cluster->cells[0].z = (vox_u16)z;
    cluster->cells[0].material = vox_world_cell(world, x, y, z)->material;
    cluster->count = 1U;
    while (cursor < cluster->count) {
        vox_structure_cell current = cluster->cells[cursor++];
        vox_i16 dx[6] = {-1, 1, 0, 0, 0, 0};
        vox_i16 dy[6] = {0, 0, -1, 1, 0, 0};
        vox_i16 dz[6] = {0, 0, 0, 0, -1, 1};
        vox_u16 direction;
        if (current.x < cluster->min_x) cluster->min_x = current.x;
        if (current.x > cluster->max_x) cluster->max_x = current.x;
        if (current.y < cluster->min_y) cluster->min_y = current.y;
        if (current.y > cluster->max_y) cluster->max_y = current.y;
        if (current.z < cluster->min_z) cluster->min_z = current.z;
        if (current.z > cluster->max_z) cluster->max_z = current.z;
        if (current.y + 1U >= VOX_WORLD_HEIGHT) {
            cluster->anchored = 1U;
        }
        if (cluster->load_q8 < 65535U) cluster->load_q8++;
        for (direction = 0U; direction < 6U; ++direction) {
            vox_i32 nx = (vox_i32)current.x + dx[direction];
            vox_i32 ny = (vox_i32)current.y + dy[direction];
            vox_i32 nz = (vox_i32)current.z + dz[direction];
            const vox_cell *cell;
            if (nx < 0L || ny < 0L || nz < 0L ||
                nx >= (vox_i32)VOX_WORLD_WIDTH ||
                ny >= (vox_i32)VOX_WORLD_HEIGHT ||
                nz >= (vox_i32)VOX_WORLD_DEPTH ||
                cluster_contains(cluster, (vox_u16)nx, (vox_u16)ny,
                                 (vox_u16)nz)) continue;
            cell = vox_world_cell(world, (vox_u32)nx, (vox_u32)ny,
                                  (vox_u32)nz);
            if (!cluster_structural_cell(cell)) {
                if (cell != 0 && cell->material == VOX_MAT_BEDROCK) {
                    cluster->anchored = 1U;
                }
                continue;
            }
            if (cluster->count >= VOX_CLUSTER_MAX_CELLS) {
                overflow = 1U;
                continue;
            }
            cluster->cells[cluster->count].x = (vox_u16)nx;
            cluster->cells[cluster->count].y = (vox_u16)ny;
            cluster->cells[cluster->count].z = (vox_u16)nz;
            cluster->cells[cluster->count].material = cell->material;
            cluster->count++;
        }
    }
    cluster->complete = overflow == 0U ? 1U : 0U;
    if (!cluster->complete) return VOX_ERR_CAPACITY;
    cluster->support_q8 = cluster->anchored != 0U ? 255U : 0U;
    if (cluster->anchored != 0U) return VOX_ERR_COLLISION;
    for (cursor = 0U; cursor < cluster->count; ++cursor) {
        (void)vox_world_set(world, cluster->cells[cursor].x,
                             cluster->cells[cursor].y,
                             cluster->cells[cursor].z, VOX_MAT_AIR, 0L);
    }
    return VOX_OK;
}

vox_result vox_cluster_extract_unsupported(vox_world *world, vox_u32 x,
                                           vox_u32 y, vox_u32 z,
                                           vox_structure_cluster *cluster)
{
    vox_u16 cursor = 0U;
    vox_u16 overflow = 0U;
    if (world == 0 || cluster == 0 || world->abi_version != VOX_ABI_VERSION ||
        !cluster_in_bounds(x, y, z)) return VOX_ERR_INVALID;
    vox_cluster_init(cluster);
    if (vox_world_cell(world, x, y, z) == 0 ||
        !cluster_structural_cell(vox_world_cell(world, x, y, z))) {
        cluster->complete = 1U;
        return VOX_OK;
    }
    /* A supported neighbour is a fracture boundary, not an invitation to
     * walk all the way through a mountain until bedrock is discovered. */
    if (structure_supported(world, x, y, z)) {
        cluster->complete = 1U;
        cluster->anchored = 1U;
        cluster->support_q8 = 255U;
        return VOX_ERR_COLLISION;
    }
    cluster->cells[0].x = (vox_u16)x;
    cluster->cells[0].y = (vox_u16)y;
    cluster->cells[0].z = (vox_u16)z;
    cluster->cells[0].material = vox_world_cell(world, x, y, z)->material;
    cluster->count = 1U;
    while (cursor < cluster->count) {
        vox_structure_cell current = cluster->cells[cursor++];
        vox_i16 dx[6] = {-1, 1, 0, 0, 0, 0};
        vox_i16 dy[6] = {0, 0, -1, 1, 0, 0};
        vox_i16 dz[6] = {0, 0, 0, 0, -1, 1};
        vox_u16 direction;
        if (current.x < cluster->min_x) cluster->min_x = current.x;
        if (current.x > cluster->max_x) cluster->max_x = current.x;
        if (current.y < cluster->min_y) cluster->min_y = current.y;
        if (current.y > cluster->max_y) cluster->max_y = current.y;
        if (current.z < cluster->min_z) cluster->min_z = current.z;
        if (current.z > cluster->max_z) cluster->max_z = current.z;
        if (cluster->load_q8 < 65535U) cluster->load_q8++;
        for (direction = 0U; direction < 6U; ++direction) {
            vox_i32 nx = (vox_i32)current.x + dx[direction];
            vox_i32 ny = (vox_i32)current.y + dy[direction];
            vox_i32 nz = (vox_i32)current.z + dz[direction];
            const vox_cell *cell;
            if (nx < 0L || ny < 0L || nz < 0L ||
                nx >= (vox_i32)VOX_WORLD_WIDTH ||
                ny >= (vox_i32)VOX_WORLD_HEIGHT ||
                nz >= (vox_i32)VOX_WORLD_DEPTH ||
                cluster_contains(cluster, (vox_u16)nx, (vox_u16)ny,
                                 (vox_u16)nz)) continue;
            cell = vox_world_cell(world, (vox_u32)nx, (vox_u32)ny,
                                  (vox_u32)nz);
            if (!cluster_structural_cell(cell) ||
                structure_supported(world, (vox_u32)nx, (vox_u32)ny,
                                    (vox_u32)nz)) {
                continue;
            }
            if (cluster->count >= VOX_CLUSTER_MAX_CELLS) {
                overflow = 1U;
                continue;
            }
            cluster->cells[cluster->count].x = (vox_u16)nx;
            cluster->cells[cluster->count].y = (vox_u16)ny;
            cluster->cells[cluster->count].z = (vox_u16)nz;
            cluster->cells[cluster->count].material = cell->material;
            cluster->count++;
        }
    }
    cluster->complete = overflow == 0U ? 1U : 0U;
    cluster->anchored = 0U;
    cluster->support_q8 = 0U;
    for (cursor = 0U; cursor < cluster->count; ++cursor) {
        (void)vox_world_set(world, cluster->cells[cursor].x,
                             cluster->cells[cursor].y,
                             cluster->cells[cursor].z, VOX_MAT_AIR, 0L);
    }
    return VOX_OK;
}

vox_result vox_cluster_restore(vox_world *world,
                               const vox_structure_cluster *cluster,
                               vox_u16 loose)
{
    vox_u16 i;
    if (world == 0 || cluster == 0 || world->abi_version != VOX_ABI_VERSION ||
        loose > 1U) return VOX_ERR_INVALID;
    for (i = 0U; i < cluster->count; ++i) {
        const vox_cell *cell = vox_world_cell(world, cluster->cells[i].x,
                                              cluster->cells[i].y,
                                              cluster->cells[i].z);
        if (cell == 0 || cell->material != VOX_MAT_AIR) {
            return VOX_ERR_COLLISION;
        }
    }
    for (i = 0U; i < cluster->count; ++i) {
        if (vox_world_set(world, cluster->cells[i].x, cluster->cells[i].y,
                          cluster->cells[i].z, cluster->cells[i].material,
                          0L) != VOX_OK) return VOX_ERR_INVALID;
        if (loose != 0U &&
            vox_world_set_loose(world, cluster->cells[i].x,
                                cluster->cells[i].y,
                                cluster->cells[i].z, 1U) != VOX_OK) {
            return VOX_ERR_INVALID;
        }
    }
    return VOX_OK;
}

vox_result vox_cluster_spawn_debris(const vox_structure_cluster *cluster,
                                    vox_rigid_world *rigid,
                                    vox_i32 impulse_x_q16,
                                    vox_i32 impulse_y_q16)
{
    vox_u16 body_index;
    vox_i32 center_x;
    vox_i32 center_y;
    vox_i32 width;
    vox_i32 height;
    if (cluster == 0 || rigid == 0 || cluster->count == 0U) {
        return VOX_ERR_INVALID;
    }
    center_x = ((vox_i32)cluster->min_x + (vox_i32)cluster->max_x) * 32768L;
    center_y = ((vox_i32)cluster->min_y + (vox_i32)cluster->max_y) * 32768L;
    width = ((vox_i32)cluster->max_x - (vox_i32)cluster->min_x + 1L) * 32768L;
    height = ((vox_i32)cluster->max_y - (vox_i32)cluster->min_y + 1L) * 32768L;
    if (vox_rigid_spawn(rigid, &body_index, center_x, center_y,
                        width, height, (vox_i32)cluster->count << 16,
                        VOX_RIGID_BODY_DEBRIS) != VOX_OK) {
        return VOX_ERR_CAPACITY;
    }
    rigid->bodies[body_index].velocity_x_q16 = impulse_x_q16;
    rigid->bodies[body_index].velocity_y_q16 = impulse_y_q16;
    rigid->bodies[body_index].angular_velocity_q16 = impulse_x_q16 / 4L;
    return VOX_OK;
}

void vox_structure_init(vox_structure_state *state)
{
    vox_u16 i;
    if (state == 0) return;
    state->abi_version = VOX_ABI_VERSION;
    state->struct_size = (vox_u32)sizeof(*state);
    state->tick = 0U;
    state->cascade_count = 0U;
    state->discarded_frontier = 0U;
    state->discarded_collapse = 0U;
    state->frontier_head = 0U;
    state->frontier_count = (vox_u16)VOX_STRUCTURE_FRONTIER_CAPACITY;
    state->frontier_cursor = 0U;
    state->collapse_head = 0U;
    state->collapse_count = 0U;
    state->reserved = 0U;
    for (i = 0U; i < VOX_STRUCTURE_FRONTIER_CAPACITY; ++i) {
        state->frontier[i] = i;
    }
    for (i = 0U; i < VOX_WORLD_CHUNK_COUNT; ++i) {
        state->chunks[i].support_q8 = 255U;
        state->chunks[i].load_q8 = 0U;
        state->chunks[i].dirty = 1U;
        state->chunks[i].collapse_risk_q8 = 0U;
        state->chunks[i].collapse_armed = 0U;
        state->chunks[i].collapse_pending = 0U;
        state->chunks[i].source = VOX_STRUCTURE_NO_SOURCE;
        state->chunks[i].weapon = 0U;
    }
    for (i = 0U; i < VOX_STRUCTURE_COLLAPSE_CAPACITY; ++i) {
        state->collapses[i].x = 0U;
        state->collapses[i].y = 0U;
        state->collapses[i].z = 0U;
        state->collapses[i].chunk_index = 0U;
        state->collapses[i].source = VOX_STRUCTURE_NO_SOURCE;
        state->collapses[i].weapon = 0U;
        state->collapses[i].risk_q8 = 0U;
        state->collapses[i].reserved = 0U;
    }
}

vox_result vox_structure_invalidate(vox_structure_state *state,
                                    vox_u32 x, vox_u32 y, vox_u32 radius)
{
    return vox_structure_invalidate_with_cause(
        state, x, y, radius, VOX_STRUCTURE_NO_SOURCE, 0U);
}

vox_result vox_structure_invalidate_with_cause(vox_structure_state *state,
                                               vox_u32 x, vox_u32 y,
                                               vox_u32 radius,
                                               vox_u16 source,
                                               vox_u16 weapon)
{
    vox_i32 min_x;
    vox_i32 max_x;
    vox_i32 min_y;
    vox_i32 max_y;
    vox_i32 chunk_y;
    vox_i32 chunk_x;
    if (state == 0 || state->abi_version != VOX_ABI_VERSION ||
        state->struct_size < (vox_u32)sizeof(*state) ||
        x >= VOX_WORLD_WIDTH || y >= VOX_WORLD_HEIGHT) return VOX_ERR_INVALID;
    min_x = (vox_i32)x - (vox_i32)radius;
    max_x = (vox_i32)x + (vox_i32)radius;
    min_y = (vox_i32)y - (vox_i32)radius;
    max_y = (vox_i32)y + (vox_i32)radius;
    if (min_x < 0L) min_x = 0L;
    if (min_y < 0L) min_y = 0L;
    if (max_x >= (vox_i32)VOX_WORLD_WIDTH) {
        max_x = (vox_i32)VOX_WORLD_WIDTH - 1L;
    }
    if (max_y >= (vox_i32)VOX_WORLD_HEIGHT) {
        max_y = (vox_i32)VOX_WORLD_HEIGHT - 1L;
    }
    for (chunk_y = min_y / (vox_i32)VOX_CHUNK_HEIGHT;
         chunk_y <= max_y / (vox_i32)VOX_CHUNK_HEIGHT; ++chunk_y) {
        for (chunk_x = min_x / (vox_i32)VOX_CHUNK_WIDTH;
             chunk_x <= max_x / (vox_i32)VOX_CHUNK_WIDTH; ++chunk_x) {
            vox_u16 index = structure_chunk_index((vox_u32)chunk_x,
                                                   (vox_u32)chunk_y);
            structure_mark_chunk(state, index, source, weapon, 1U);
        }
    }
    return VOX_OK;
}

vox_result vox_structure_step(vox_structure_state *state,
                              const vox_world *world, vox_u16 max_chunks)
{
    vox_u16 processed = 0U;
    if (state == 0 || world == 0 ||
        state->abi_version != VOX_ABI_VERSION ||
        state->struct_size < (vox_u32)sizeof(*state) ||
        world->abi_version != VOX_ABI_VERSION || max_chunks == 0U) {
        return VOX_ERR_INVALID;
    }
    while (processed < max_chunks && state->frontier_count > 0U) {
        vox_u16 index = state->frontier[state->frontier_head];
        vox_u32 chunk_x = index % VOX_WORLD_CHUNKS_X;
        vox_u32 chunk_y = index / VOX_WORLD_CHUNKS_X;
        vox_u32 x;
        vox_u32 y;
        vox_u32 z;
        vox_u32 structural = 0U;
        vox_u32 supported = 0U;
        vox_u32 unsupported = 0U;
        vox_u16 old_risk = state->chunks[index].collapse_risk_q8;
        vox_u16 collapse_armed = state->chunks[index].collapse_armed;
        vox_u16 candidate_x = 0U;
        vox_u16 candidate_y = 0U;
        vox_u16 candidate_z = 0U;
        vox_u16 candidate_found = 0U;
        state->frontier_head = (vox_u16)((state->frontier_head + 1U) %
                                         VOX_STRUCTURE_FRONTIER_CAPACITY);
        state->frontier_count--;
        state->chunks[index].dirty = 0U;
        for (y = chunk_y * VOX_CHUNK_HEIGHT;
             y < (chunk_y + 1U) * VOX_CHUNK_HEIGHT; ++y) {
            for (x = chunk_x * VOX_CHUNK_WIDTH;
                 x < (chunk_x + 1U) * VOX_CHUNK_WIDTH; ++x) {
                for (z = 0U; z < VOX_WORLD_DEPTH; ++z) {
                    const vox_cell *cell = vox_world_cell(world, x, y, z);
                    if (!cluster_structural_cell(cell) ||
                        (cell->flags & VOX_CELL_LOOSE) != 0U) continue;
                    structural++;
                    if (structure_supported(world, x, y, z)) {
                        supported++;
                    } else {
                        unsupported++;
                        if (candidate_found == 0U) {
                            candidate_x = (vox_u16)x;
                            candidate_y = (vox_u16)y;
                            candidate_z = (vox_u16)z;
                            candidate_found = 1U;
                        }
                    }
                }
            }
        }
        state->chunks[index].load_q8 = structural > 255U ? 255U :
                                       (vox_u16)structural;
        if (structural == 0U) {
            state->chunks[index].support_q8 = 255U;
            state->chunks[index].collapse_risk_q8 = 0U;
        } else {
            state->chunks[index].support_q8 = (vox_u16)(
                (supported * 255U) / structural);
            state->chunks[index].collapse_risk_q8 = (vox_u16)(
                (unsupported * 255U) / structural);
        }
        if (old_risk == 0U && state->chunks[index].collapse_risk_q8 != 0U) {
            state->cascade_count++;
        }
        if (collapse_armed != 0U && candidate_found != 0U) {
            structure_queue_collapse(state, index, candidate_x, candidate_y,
                                     candidate_z,
                                     state->chunks[index].collapse_risk_q8);
        }
        state->chunks[index].collapse_armed = 0U;
        state->frontier_cursor = index;
        processed++;
    }
    state->tick++;
    return VOX_OK;
}

vox_result vox_structure_pop_collapse(vox_structure_state *state,
                                      vox_structure_collapse *collapse)
{
    vox_u16 slot;
    vox_u16 chunk_index;
    if (state == 0 || collapse == 0 ||
        state->abi_version != VOX_ABI_VERSION ||
        state->struct_size < (vox_u32)sizeof(*state)) {
        return VOX_ERR_INVALID;
    }
    if (state->collapse_count == 0U) return VOX_ERR_CAPACITY;
    slot = state->collapse_head;
    *collapse = state->collapses[slot];
    chunk_index = collapse->chunk_index;
    if (chunk_index < VOX_WORLD_CHUNK_COUNT) {
        state->chunks[chunk_index].collapse_pending = 0U;
    }
    state->collapses[slot].x = 0U;
    state->collapses[slot].y = 0U;
    state->collapses[slot].z = 0U;
    state->collapses[slot].chunk_index = 0U;
    state->collapses[slot].source = VOX_STRUCTURE_NO_SOURCE;
    state->collapses[slot].weapon = 0U;
    state->collapses[slot].risk_q8 = 0U;
    state->collapses[slot].reserved = 0U;
    state->collapse_head = (vox_u16)((state->collapse_head + 1U) %
                                     VOX_STRUCTURE_COLLAPSE_CAPACITY);
    state->collapse_count--;
    return VOX_OK;
}

vox_u32 vox_structure_hash(const vox_structure_state *state)
{
    vox_u32 hash = 2166136261U;
    vox_u16 i;
    if (state == 0) return 0U;
    hash = structure_hash_mix(hash, state->tick);
    hash = structure_hash_mix(hash, state->cascade_count);
    hash = structure_hash_mix(hash, state->discarded_frontier);
    hash = structure_hash_mix(hash, state->discarded_collapse);
    hash = structure_hash_mix(hash, state->frontier_head);
    hash = structure_hash_mix(hash, state->frontier_count);
    hash = structure_hash_mix(hash, state->frontier_cursor);
    hash = structure_hash_mix(hash, state->collapse_head);
    hash = structure_hash_mix(hash, state->collapse_count);
    for (i = 0U; i < VOX_STRUCTURE_FRONTIER_CAPACITY; ++i) {
        hash = structure_hash_mix(hash, state->frontier[i]);
    }
    for (i = 0U; i < VOX_STRUCTURE_COLLAPSE_CAPACITY; ++i) {
        const vox_structure_collapse *collapse = &state->collapses[i];
        hash = structure_hash_mix(hash, collapse->x);
        hash = structure_hash_mix(hash, collapse->y);
        hash = structure_hash_mix(hash, collapse->z);
        hash = structure_hash_mix(hash, collapse->chunk_index);
        hash = structure_hash_mix(hash, collapse->source);
        hash = structure_hash_mix(hash, collapse->weapon);
        hash = structure_hash_mix(hash, collapse->risk_q8);
    }
    for (i = 0U; i < VOX_WORLD_CHUNK_COUNT; ++i) {
        hash = structure_hash_mix(hash, state->chunks[i].support_q8);
        hash = structure_hash_mix(hash, state->chunks[i].load_q8);
        hash = structure_hash_mix(hash, state->chunks[i].dirty);
        hash = structure_hash_mix(hash, state->chunks[i].collapse_risk_q8);
        hash = structure_hash_mix(hash, state->chunks[i].collapse_armed);
        hash = structure_hash_mix(hash, state->chunks[i].collapse_pending);
        hash = structure_hash_mix(hash, state->chunks[i].source);
        hash = structure_hash_mix(hash, state->chunks[i].weapon);
    }
    return hash;
}
