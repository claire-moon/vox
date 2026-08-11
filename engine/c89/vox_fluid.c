/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "vox/vox_fluid.h"

#define VOX_FLUID_WATER_TEMP (20L << 16)
#define VOX_FLUID_REACTION_VOLUME 8192L
#define VOX_FLUID_LATERAL_DIVISOR 4L

static int fluid_valid(vox_u16 x, vox_u16 y, vox_u16 z)
{
    return x < VOX_FLUID_GRID_WIDTH && y < VOX_FLUID_GRID_HEIGHT &&
           z < VOX_FLUID_GRID_DEPTH;
}

static int fluid_same(const vox_fluid_cell *cell, vox_u16 x, vox_u16 y,
                      vox_u16 z)
{
    return cell->active != 0U && cell->x == x && cell->y == y &&
           cell->z == z;
}

static vox_u32 fluid_coordinate_key(vox_u16 x, vox_u16 y, vox_u16 z)
{
    return ((vox_u32)y << 20) | ((vox_u32)x << 10) | (vox_u32)z;
}

static vox_u16 fluid_lookup_slot(vox_u32 key)
{
    key ^= key >> 16;
    key *= 2654435761UL;
    return (vox_u16)(key & (VOX_FLUID_LOOKUP_CAPACITY - 1U));
}

static void fluid_lookup_insert(vox_fluid_world *world, vox_u16 index)
{
    vox_u16 probe;
    vox_u16 slot = fluid_lookup_slot(
        fluid_coordinate_key(world->cells[index].x,
                             world->cells[index].y,
                             world->cells[index].z));
    for (probe = 0U; probe < VOX_FLUID_LOOKUP_CAPACITY; ++probe) {
        if (world->lookup[slot] == 0U) {
            world->lookup[slot] = (vox_u16)(index + 1U);
            return;
        }
        slot = (vox_u16)((slot + 1U) % VOX_FLUID_LOOKUP_CAPACITY);
    }
}

static void fluid_lookup_rebuild(vox_fluid_world *world)
{
    vox_u16 i;
    for (i = 0U; i < VOX_FLUID_LOOKUP_CAPACITY; ++i) {
        world->lookup[i] = 0U;
    }
    for (i = 0U; i < world->active_cells && i < VOX_FLUID_MAX_CELLS;
         ++i) {
        fluid_lookup_insert(world, i);
    }
}

static vox_u32 fluid_key(const vox_fluid_cell *cell);

static vox_u16 fluid_find(const vox_fluid_world *world, vox_u16 x,
                          vox_u16 y, vox_u16 z)
{
    vox_u16 probe;
    vox_u16 slot;
    vox_u32 key = fluid_coordinate_key(x, y, z);
    slot = fluid_lookup_slot(key);
    for (probe = 0U; probe < VOX_FLUID_LOOKUP_CAPACITY; ++probe) {
        vox_u16 entry = world->lookup[slot];
        vox_u16 index;
        if (entry == 0U) return (vox_u16)VOX_FLUID_MAX_CELLS;
        index = (vox_u16)(entry - 1U);
        if (index < VOX_FLUID_MAX_CELLS &&
            fluid_same(&world->cells[index], x, y, z)) {
            return index;
        }
        slot = (vox_u16)((slot + 1U) % VOX_FLUID_LOOKUP_CAPACITY);
    }
    return (vox_u16)VOX_FLUID_MAX_CELLS;
}

static vox_u16 fluid_free_slot(const vox_fluid_world *world)
{
    vox_u16 i;
    for (i = 0U; i < VOX_FLUID_MAX_CELLS; ++i) {
        if (world->cells[i].active == 0U) return i;
    }
    return (vox_u16)VOX_FLUID_MAX_CELLS;
}

static vox_u32 fluid_key(const vox_fluid_cell *cell)
{
    /* Process the upper cells before their destinations.  That gives a
     * proposal one bounded downward step per tick and lets water/lava see
     * contact before the lower source can move away. */
    if (cell->active == 0U) return 4294967295UL;
    return fluid_coordinate_key(cell->x, cell->y, cell->z);
}

static void fluid_sort_range(vox_fluid_cell *cells, vox_i32 left,
                             vox_i32 right)
{
    vox_i32 i = left;
    vox_i32 j = right;
    vox_u32 pivot = fluid_key(&cells[left + (right - left) / 2L]);
    if (left >= right) return;
    while (i <= j) {
        while (fluid_key(&cells[i]) < pivot) ++i;
        while (fluid_key(&cells[j]) > pivot) --j;
        if (i <= j) {
            vox_fluid_cell value = cells[i];
            cells[i] = cells[j];
            cells[j] = value;
            ++i;
            --j;
        }
    }
    if (left < j) fluid_sort_range(cells, left, j);
    if (i < right) fluid_sort_range(cells, i, right);
}

static void fluid_merge_sorted(vox_fluid_world *world, vox_u16 middle,
                                vox_u16 active)
{
    vox_u16 left = 0U;
    vox_u16 right = middle;
    vox_u16 output = 0U;
    while (left < middle && right < active) {
        if (fluid_key(&world->cells[left]) <=
            fluid_key(&world->cells[right])) {
            world->sort_scratch[output++] = world->cells[left++];
        } else {
            world->sort_scratch[output++] = world->cells[right++];
        }
    }
    while (left < middle) world->sort_scratch[output++] =
        world->cells[left++];
    while (right < active) world->sort_scratch[output++] =
        world->cells[right++];
    for (output = 0U; output < active; ++output) {
        world->cells[output] = world->sort_scratch[output];
    }
}

static void fluid_sort(vox_fluid_world *world)
{
    vox_u16 i;
    vox_u16 active = 0U;
    vox_u16 previous_sorted = world->sorted_cells;
    vox_u16 sorted_active = 0U;
    if (previous_sorted > VOX_FLUID_MAX_CELLS) {
        previous_sorted = VOX_FLUID_MAX_CELLS;
    }
    for (i = 0U; i < VOX_FLUID_MAX_CELLS; ++i) {
        if (world->cells[i].active != 0U) {
            if (i != active) world->cells[active] = world->cells[i];
            if (i < previous_sorted) ++sorted_active;
            ++active;
        }
    }
    for (i = active; i < VOX_FLUID_MAX_CELLS; ++i) {
        world->cells[i].active = 0U;
        world->cells[i].material = VOX_FLUID_NONE;
        world->cells[i].volume_q16 = 0L;
        world->cells[i].pressure_q16 = 0L;
        world->cells[i].flow_q16 = 0L;
    }
    world->active_cells = active;
    if (sorted_active < active) {
        if ((vox_u32)active - (vox_u32)sorted_active > 1U) {
            fluid_sort_range(world->cells, (vox_i32)sorted_active,
                             (vox_i32)active - 1L);
        }
        fluid_merge_sorted(world, sorted_active, active);
    }
    world->sorted_cells = active;
    fluid_lookup_rebuild(world);
}

static void fluid_refresh(vox_fluid_world *world)
{
    vox_u16 i;
    vox_u32 active = 0U;
    vox_i32 total = 0L;
    for (i = 0U; i < VOX_FLUID_MAX_CELLS; ++i) {
        vox_fluid_cell *cell = &world->cells[i];
        if (cell->active == 0U || cell->volume_q16 <= 0L) {
            cell->active = 0U;
            cell->material = VOX_FLUID_NONE;
            cell->volume_q16 = 0L;
            cell->pressure_q16 = 0L;
            cell->flow_q16 = 0L;
        } else {
            vox_i32 head_q16 = (vox_i32)cell->y << 8;
            cell->active = 1U;
            /* A deeper cell has a slightly larger head.  It is derived
             * state, so it cannot introduce or remove mass. */
            if (cell->volume_q16 > 2147483647L - head_q16) {
                cell->pressure_q16 = 2147483647L;
            } else {
                cell->pressure_q16 = cell->volume_q16 + head_q16;
            }
            ++active;
            total += cell->volume_q16;
        }
    }
    world->active_cells = active;
    world->total_volume_q16 = total;
}

void vox_fluid_init(vox_fluid_world *world)
{
    vox_u16 i;
    if (world == 0) return;
    world->abi_version = VOX_ABI_VERSION;
    world->struct_size = (vox_u32)sizeof(*world);
    world->tick = 0U;
    world->active_cells = 0U;
    world->frontier_cursor = 0U;
    world->sorted_cells = 0U;
    world->total_volume_q16 = 0L;
    world->reaction_loss_q16 = 0L;
    for (i = 0U; i < VOX_FLUID_MAX_CELLS; ++i) {
        world->cells[i].x = 0U;
        world->cells[i].y = 0U;
        world->cells[i].z = 0U;
        world->cells[i].material = VOX_FLUID_NONE;
        world->cells[i].active = 0U;
        world->cells[i].volume_q16 = 0L;
        world->cells[i].pressure_q16 = 0L;
        world->cells[i].temperature_q16 = VOX_FLUID_WATER_TEMP;
        world->cells[i].flow_q16 = 0L;
        world->sort_scratch[i].x = 0U;
        world->sort_scratch[i].y = 0U;
        world->sort_scratch[i].z = 0U;
        world->sort_scratch[i].material = VOX_FLUID_NONE;
        world->sort_scratch[i].active = 0U;
        world->sort_scratch[i].volume_q16 = 0L;
        world->sort_scratch[i].pressure_q16 = 0L;
        world->sort_scratch[i].temperature_q16 = VOX_FLUID_WATER_TEMP;
        world->sort_scratch[i].flow_q16 = 0L;
    }
    for (i = 0U; i < VOX_FLUID_LOOKUP_CAPACITY; ++i) {
        world->lookup[i] = 0U;
    }
}

static vox_result fluid_set_at(vox_fluid_world *world, vox_u16 x, vox_u16 y,
                               vox_u16 z, vox_u16 material,
                               vox_i32 volume_q16, vox_i32 temperature_q16)
{
    vox_u16 index;
    if (world == 0 || world->abi_version != VOX_ABI_VERSION ||
        world->struct_size < (vox_u32)sizeof(*world) ||
        !fluid_valid(x, y, z) || material > VOX_FLUID_BLOOD ||
        (material == VOX_FLUID_NONE && volume_q16 != 0L) ||
        volume_q16 < 0L || volume_q16 > VOX_FLUID_CELL_CAPACITY_Q16) {
        return VOX_ERR_INVALID;
    }
    index = fluid_find(world, x, y, z);
    if (index == (vox_u16)VOX_FLUID_MAX_CELLS && volume_q16 > 0L) {
        index = fluid_free_slot(world);
        if (index == (vox_u16)VOX_FLUID_MAX_CELLS) return VOX_ERR_CAPACITY;
        world->cells[index].x = x;
        world->cells[index].y = y;
        world->cells[index].z = z;
        world->cells[index].active = 1U;
    }
    if (index != (vox_u16)VOX_FLUID_MAX_CELLS) {
        world->cells[index].material = material;
        world->cells[index].volume_q16 = volume_q16;
        world->cells[index].temperature_q16 = temperature_q16;
        world->cells[index].flow_q16 = 0L;
    }
    fluid_refresh(world);
    fluid_sort(world);
    return VOX_OK;
}

vox_result vox_fluid_set_at(vox_fluid_world *world, vox_u16 x, vox_u16 y,
                            vox_u16 z, vox_u16 material, vox_i32 volume_q16,
                            vox_i32 temperature_q16)
{
    return fluid_set_at(world, x, y, z, material, volume_q16,
                        temperature_q16);
}

vox_result vox_fluid_set(vox_fluid_world *world, vox_u16 x, vox_u16 y,
                         vox_u16 material, vox_i32 volume_q16,
                         vox_i32 temperature_q16)
{
    return fluid_set_at(world, x, y, 0U, material, volume_q16,
                        temperature_q16);
}

static vox_result fluid_add_at(vox_fluid_world *world, vox_u16 x, vox_u16 y,
                               vox_u16 z, vox_u16 material,
                               vox_i32 volume_q16, vox_i32 temperature_q16)
{
    vox_u16 index;
    if (world == 0 || world->abi_version != VOX_ABI_VERSION ||
        world->struct_size < (vox_u32)sizeof(*world) ||
        !fluid_valid(x, y, z) || material == VOX_FLUID_NONE ||
        material > VOX_FLUID_BLOOD || volume_q16 <= 0L) return VOX_ERR_INVALID;
    index = fluid_find(world, x, y, z);
    if (index == (vox_u16)VOX_FLUID_MAX_CELLS) {
        index = fluid_free_slot(world);
        if (index == (vox_u16)VOX_FLUID_MAX_CELLS) return VOX_ERR_CAPACITY;
        world->cells[index].x = x;
        world->cells[index].y = y;
        world->cells[index].z = z;
        world->cells[index].active = 1U;
        world->cells[index].material = VOX_FLUID_NONE;
        world->cells[index].volume_q16 = 0L;
        if (world->active_cells < VOX_FLUID_MAX_CELLS) {
            world->active_cells++;
        }
        fluid_lookup_insert(world, index);
    }
    if (world->cells[index].material != VOX_FLUID_NONE &&
        world->cells[index].material != material) return VOX_ERR_COLLISION;
    if (world->cells[index].volume_q16 >
        VOX_FLUID_CELL_CAPACITY_Q16 - volume_q16) return VOX_ERR_CAPACITY;
    world->cells[index].material = material;
    world->cells[index].volume_q16 += volume_q16;
    world->cells[index].temperature_q16 = temperature_q16;
    fluid_refresh(world);
    fluid_sort(world);
    return VOX_OK;
}

vox_result vox_fluid_add_at(vox_fluid_world *world, vox_u16 x, vox_u16 y,
                            vox_u16 z, vox_u16 material, vox_i32 volume_q16,
                            vox_i32 temperature_q16)
{
    return fluid_add_at(world, x, y, z, material, volume_q16,
                        temperature_q16);
}

vox_result vox_fluid_add(vox_fluid_world *world, vox_u16 x, vox_u16 y,
                         vox_u16 material, vox_i32 volume_q16,
                         vox_i32 temperature_q16)
{
    return fluid_add_at(world, x, y, 0U, material, volume_q16,
                        temperature_q16);
}

static int fluid_blocked(const vox_world *terrain, vox_u16 x, vox_u16 y,
                         vox_u16 z)
{
    const vox_cell *cell;
    const vox_material_properties *properties;
    if (terrain == 0) return 0;
    /* Character collision is column-wide because the playable view is
     * side-on.  Fluids are 3-D volumes, so a solid in another depth slice
     * must not dam this slice. */
    cell = vox_world_cell(terrain, x, y, z);
    if (cell == 0 || cell->material == VOX_MAT_AIR ||
        (cell->flags & VOX_CELL_LOOSE) != 0U) return 0;
    properties = vox_material_get(cell->material);
    return properties != 0 &&
           (properties->flags & VOX_MATERIAL_SOLID) != 0U;
}

static vox_i32 fluid_pressure(const vox_fluid_cell *cell)
{
    vox_i32 head_q16 = (vox_i32)cell->y << 8;
    if (cell->volume_q16 > 2147483647L - head_q16) {
        return 2147483647L;
    }
    return cell->volume_q16 + head_q16;
}

static vox_result fluid_transfer(vox_fluid_world *world,
                                 vox_fluid_cell *source, vox_u16 x,
                                 vox_u16 y, vox_u16 z, vox_i32 amount,
                                 const vox_world *terrain)
{
    vox_u16 index;
    vox_i32 available;
    if (amount <= 0L || source->active == 0U ||
        source->material == VOX_FLUID_NONE || !fluid_valid(x, y, z) ||
        fluid_blocked(terrain, x, y, z)) return VOX_OK;
    available = amount < source->volume_q16 ? amount : source->volume_q16;
    index = fluid_find(world, x, y, z);
    if (index == (vox_u16)VOX_FLUID_MAX_CELLS) {
        index = fluid_free_slot(world);
        if (index == (vox_u16)VOX_FLUID_MAX_CELLS) return VOX_ERR_CAPACITY;
        world->cells[index].x = x;
        world->cells[index].y = y;
        world->cells[index].z = z;
        world->cells[index].active = 1U;
        world->cells[index].material = VOX_FLUID_NONE;
        world->cells[index].volume_q16 = 0L;
        if (world->active_cells < VOX_FLUID_MAX_CELLS) {
            world->active_cells++;
        }
        fluid_lookup_insert(world, index);
    }
    if (world->cells[index].material != VOX_FLUID_NONE &&
        world->cells[index].material != source->material) return VOX_OK;
    if (available > VOX_FLUID_CELL_CAPACITY_Q16 -
                   world->cells[index].volume_q16) {
        available = VOX_FLUID_CELL_CAPACITY_Q16 -
                    world->cells[index].volume_q16;
    }
    if (available <= 0L) return VOX_OK;
    world->cells[index].material = source->material;
    world->cells[index].volume_q16 += available;
    world->cells[index].temperature_q16 = source->temperature_q16;
    source->volume_q16 -= available;
    source->flow_q16 += available;
    return VOX_OK;
}

static void fluid_react_pair(vox_fluid_world *world, vox_u16 first,
                             vox_u16 second)
{
    vox_fluid_cell *a;
    vox_fluid_cell *b;
    vox_i32 loss;
    if (first >= VOX_FLUID_MAX_CELLS || second >= VOX_FLUID_MAX_CELLS ||
        first == second) return;
    a = &world->cells[first];
    b = &world->cells[second];
    /* Pair visitation is based on coordinates rather than sparse slot
     * indices.  Sorting is deliberately allowed to change slot order after
     * a transfer, while each adjacent reaction must still happen once. */
    if (a->x > b->x ||
        (a->x == b->x && a->y > b->y) ||
        (a->x == b->x && a->y == b->y && a->z > b->z)) return;
    if (!a->active || !b->active ||
        !((a->material == VOX_FLUID_WATER &&
           b->material == VOX_FLUID_LAVA) ||
          (a->material == VOX_FLUID_LAVA &&
           b->material == VOX_FLUID_WATER))) return;
    loss = a->volume_q16 < b->volume_q16 ? a->volume_q16 : b->volume_q16;
    if (loss > VOX_FLUID_REACTION_VOLUME) loss = VOX_FLUID_REACTION_VOLUME;
    if (loss <= 0L) return;
    a->volume_q16 -= loss;
    b->volume_q16 -= loss;
    a->temperature_q16 += 1L << 12;
    b->temperature_q16 -= 1L << 12;
    if (world->reaction_loss_q16 > 2147483647L - loss * 2L) {
        world->reaction_loss_q16 = 2147483647L;
    } else {
        world->reaction_loss_q16 += loss * 2L;
    }
}

static void fluid_equalize_neighbor(vox_fluid_world *world,
                                    vox_fluid_cell *cell,
                                    vox_u16 x, vox_u16 y, vox_u16 z,
                                    const vox_world *terrain)
{
    vox_u16 neighbor;
    vox_i32 amount;
    if (!fluid_valid(x, y, z) || fluid_blocked(terrain, x, y, z)) return;
    neighbor = fluid_find(world, x, y, z);
    if (neighbor != (vox_u16)VOX_FLUID_MAX_CELLS) {
        if (world->cells[neighbor].material != cell->material ||
            fluid_pressure(&world->cells[neighbor]) >=
            fluid_pressure(cell)) {
            return;
        }
    }
    amount = cell->volume_q16 / VOX_FLUID_LATERAL_DIVISOR;
    (void)fluid_transfer(world, cell, x, y, z, amount, terrain);
}

vox_result vox_fluid_step_terrain(vox_fluid_world *world,
                                  const vox_world *terrain,
                                  vox_u16 max_cells)
{
    vox_u16 i;
    vox_u16 offset;
    vox_u16 active_count;
    vox_u16 start;
    vox_u16 processed = 0U;
    if (world == 0 || world->abi_version != VOX_ABI_VERSION ||
        world->struct_size < (vox_u32)sizeof(*world) ||
        (terrain != 0 && terrain->abi_version != VOX_ABI_VERSION)) {
        return VOX_ERR_INVALID;
    }
    if (max_cells == 0U) return VOX_OK;
    fluid_sort(world);
    active_count = (vox_u16)world->active_cells;
    if (active_count == 0U) {
        world->frontier_cursor = 0U;
        ++world->tick;
        return VOX_OK;
    }
    start = (vox_u16)(world->frontier_cursor % active_count);
    for (offset = 0U; offset < active_count && processed < max_cells;
         ++offset) {
        vox_fluid_cell *cell;
        vox_i32 down;
        i = (vox_u16)((start + offset) % active_count);
        cell = &world->cells[i];
        if (cell->active == 0U || cell->volume_q16 <= 0L) continue;
        ++processed;
        cell->flow_q16 = 0L;
        /* React before a full cell can move through an adjacent material.
         * Otherwise a one-cell-per-tick proposal could separate water and
         * lava before the reaction pass ever saw their contact. */
        if (cell->y + 1U < VOX_FLUID_GRID_HEIGHT) {
            vox_u16 below = fluid_find(world, cell->x,
                                       (vox_u16)(cell->y + 1U), cell->z);
            fluid_react_pair(world, i, below);
        }
        if (cell->y + 1U < VOX_FLUID_GRID_HEIGHT &&
            !fluid_blocked(terrain, cell->x,
                           (vox_u16)(cell->y + 1U), cell->z)) {
            vox_u16 below = fluid_find(world, cell->x,
                                       (vox_u16)(cell->y + 1U), cell->z);
            if (below == (vox_u16)VOX_FLUID_MAX_CELLS ||
                world->cells[below].material == cell->material) {
                down = VOX_FLUID_CELL_CAPACITY_Q16;
                if (below != (vox_u16)VOX_FLUID_MAX_CELLS)
                    down -= world->cells[below].volume_q16;
                (void)fluid_transfer(world, cell, cell->x,
                                     (vox_u16)(cell->y + 1U), cell->z,
                                     down, terrain);
            }
        }
        /* Water/lava contact consumes an explicit, accounted reaction volume.
         * The pre-transfer call above handles vertical contact; the later
         * lateral/depth calls handle equal-height contacts. */
        if (cell->x > 0U) {
            fluid_equalize_neighbor(world, cell,
                                    (vox_u16)(cell->x - 1U), cell->y,
                                    cell->z, terrain);
        }
        if (cell->x + 1U < VOX_FLUID_GRID_WIDTH) {
            fluid_equalize_neighbor(world, cell,
                                    (vox_u16)(cell->x + 1U), cell->y,
                                    cell->z, terrain);
        }
        if (cell->z > 0U) {
            fluid_equalize_neighbor(world, cell, cell->x, cell->y,
                                    (vox_u16)(cell->z - 1U), terrain);
        }
        if (cell->z + 1U < VOX_FLUID_GRID_DEPTH) {
            fluid_equalize_neighbor(world, cell, cell->x, cell->y,
                                    (vox_u16)(cell->z + 1U), terrain);
        }
        if (cell->x > 0U) {
            fluid_react_pair(world, i,
                             fluid_find(world, (vox_u16)(cell->x - 1U),
                                        cell->y, cell->z));
        }
        if (cell->x + 1U < VOX_FLUID_GRID_WIDTH) {
            fluid_react_pair(world, i,
                             fluid_find(world, (vox_u16)(cell->x + 1U),
                                        cell->y, cell->z));
        }
        if (cell->z > 0U) {
            fluid_react_pair(world, i,
                             fluid_find(world, cell->x, cell->y,
                                        (vox_u16)(cell->z - 1U)));
        }
        if (cell->z + 1U < VOX_FLUID_GRID_DEPTH) {
            fluid_react_pair(world, i,
                             fluid_find(world, cell->x, cell->y,
                                        (vox_u16)(cell->z + 1U)));
        }
    }
    fluid_refresh(world);
    fluid_sort(world);
    if (processed < active_count && world->active_cells != 0U) {
        world->frontier_cursor = (vox_u16)((start + processed) %
                                           world->active_cells);
    } else {
        world->frontier_cursor = 0U;
    }
    ++world->tick;
    return VOX_OK;
}

vox_result vox_fluid_step(vox_fluid_world *world, vox_u16 max_cells)
{
    return vox_fluid_step_terrain(world, 0, max_cells);
}

vox_i32 vox_fluid_conserved_volume(const vox_fluid_world *world)
{
    return world == 0 ? 0L : world->total_volume_q16;
}

vox_i32 vox_fluid_reaction_loss(const vox_fluid_world *world)
{
    return world == 0 ? 0L : world->reaction_loss_q16;
}

const vox_fluid_cell *vox_fluid_cell_get_at(const vox_fluid_world *world,
                                            vox_u16 x, vox_u16 y,
                                            vox_u16 z)
{
    vox_u16 index;
    if (world == 0 || !fluid_valid(x, y, z)) return 0;
    index = fluid_find(world, x, y, z);
    return index == (vox_u16)VOX_FLUID_MAX_CELLS ? 0 : &world->cells[index];
}

vox_result vox_fluid_sample_column(const vox_fluid_world *world,
                                   vox_u16 x, vox_u16 y,
                                   vox_i32 *volume_q16,
                                   vox_u16 *material)
{
    vox_u16 i;
    vox_u16 limit;
    vox_i32 total = 0L;
    vox_u16 selected = VOX_FLUID_NONE;
    if (world == 0 || world->abi_version != VOX_ABI_VERSION ||
        world->struct_size < (vox_u32)sizeof(*world) ||
        !fluid_valid(x, y, 0U) || volume_q16 == 0 || material == 0) {
        return VOX_ERR_INVALID;
    }
    limit = (vox_u16)world->active_cells;
    if (limit > VOX_FLUID_MAX_CELLS) limit = VOX_FLUID_MAX_CELLS;
    for (i = 0U; i < limit; ++i) {
        const vox_fluid_cell *cell = &world->cells[i];
        if (cell->active == 0U || cell->x != x || cell->y != y ||
            cell->volume_q16 <= 0L) {
            continue;
        }
        if (total > 2147483647L - cell->volume_q16) {
            total = 2147483647L;
        } else {
            total += cell->volume_q16;
        }
        if (cell->material == VOX_FLUID_LAVA) {
            selected = VOX_FLUID_LAVA;
        } else if (selected == VOX_FLUID_NONE) {
            selected = cell->material;
        }
    }
    *volume_q16 = total;
    *material = selected;
    return VOX_OK;
}

const vox_fluid_cell *vox_fluid_cell_get(const vox_fluid_world *world,
                                         vox_u16 x, vox_u16 y)
{
    return vox_fluid_cell_get_at(world, x, y, 0U);
}

vox_u32 vox_fluid_hash(const vox_fluid_world *world)
{
    vox_u32 hash = 2166136261U;
    vox_u16 i;
    if (world == 0) return 0U;
    hash ^= world->tick;
    hash *= 16777619U;
    hash ^= world->active_cells;
    hash *= 16777619U;
    hash ^= world->frontier_cursor;
    hash *= 16777619U;
    hash ^= world->sorted_cells;
    hash *= 16777619U;
    hash ^= (vox_u32)world->reaction_loss_q16;
    hash *= 16777619U;
    for (i = 0U; i < world->active_cells &&
         i < VOX_FLUID_MAX_CELLS; ++i) {
        const vox_fluid_cell *cell = &world->cells[i];
        hash ^= cell->x; hash *= 16777619U;
        hash ^= cell->y; hash *= 16777619U;
        hash ^= cell->z; hash *= 16777619U;
        hash ^= cell->material; hash *= 16777619U;
        hash ^= (vox_u32)cell->volume_q16; hash *= 16777619U;
        hash ^= (vox_u32)cell->pressure_q16; hash *= 16777619U;
        hash ^= (vox_u32)cell->temperature_q16; hash *= 16777619U;
        hash ^= (vox_u32)cell->flow_q16; hash *= 16777619U;
    }
    return hash;
}
