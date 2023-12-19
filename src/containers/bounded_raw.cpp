#include <gvox/containers/bounded_raw.h>
#include <gvox/stream.h>
#include <gvox/format.h>

#include <cstdlib>
#include <cstdint>

#include <new>
#include <vector>
#include <cstddef>
#include <algorithm>

#include "raw_helper.hpp"

struct GvoxBoundedRawContainer {
    struct Iterator {
        uint64_t index;
        uint64_t max_index;
        std::vector<int64_t> offset_buffer;
        GvoxOffsetMut voxel_pos;
        GvoxRangeMut range;
    };

    GvoxVoxelDesc voxel_desc{};
    GvoxExtent extent{};
    void *pre_allocated_buffer{};

    std::vector<int64_t> pre_alloc_offset_buffer{};
};

void gvox_container_raw_create_input_iterator(void *self_ptr, void **out_iterator_ptr) {
    auto &self = *static_cast<GvoxBoundedRawContainer *>(self_ptr);
    auto &iter = *new GvoxBoundedRawContainer::Iterator({
        .index = ~uint64_t{0},
        .max_index = 0,
        .offset_buffer = std::vector<int64_t>(static_cast<size_t>(self.extent.axis_n * 3)),
        .voxel_pos = {.axis_n = self.extent.axis_n},
        .range = {
            .offset = {.axis_n = self.extent.axis_n},
            .extent = {.axis_n = self.extent.axis_n},
        },
    });
    iter.voxel_pos.axis = iter.offset_buffer.data() + static_cast<ptrdiff_t>(self.extent.axis_n * 0);
    iter.range.offset.axis = iter.offset_buffer.data() + static_cast<ptrdiff_t>(self.extent.axis_n * 1);
    iter.range.extent.axis = reinterpret_cast<uint64_t *>(iter.offset_buffer.data() + static_cast<ptrdiff_t>(self.extent.axis_n * 2));
    uint64_t stride = 1;
    for (uint32_t i = 0; i < self.extent.axis_n; ++i) {
        iter.max_index += self.extent.axis[i] * stride;
        stride *= self.extent.axis[i];
    }
    (*out_iterator_ptr) = &iter;
}

void gvox_container_raw_iterator_advance(void *self_ptr, void *iterator_ptr, GvoxIteratorValue *out) {
    auto &self = *static_cast<GvoxBoundedRawContainer *>(self_ptr);
    auto &iter_self = *static_cast<GvoxBoundedRawContainer::Iterator *>(iterator_ptr);
    if (iter_self.index == ~uint64_t{0}) {
        out->tag = GVOX_ITERATOR_VALUE_TYPE_NODE_BEGIN;
        out->range = static_cast<GvoxRange>(iter_self.range);
        iter_self.index = 0;
    } else if (iter_self.index < iter_self.max_index) {
        out->tag = GVOX_ITERATOR_VALUE_TYPE_LEAF;
        auto *voxel_ptr = static_cast<uint8_t *>(self.pre_allocated_buffer) + iter_self.index;
        out->voxel_data = voxel_ptr;
    } else {
        out->tag = GVOX_ITERATOR_VALUE_TYPE_NULL;
    }
    ++iter_self.index;
}

void gvox_container_raw_destroy_iterator(void *iterator_ptr) {
    auto *iterator = static_cast<GvoxBoundedRawContainer::Iterator *>(iterator_ptr);
    delete iterator;
}

auto gvox_container_bounded_raw_description() GVOX_FUNC_ATTRIB->GvoxContainerDescription {
    return GvoxContainerDescription{
        .create = [](void **out_self, GvoxContainerCreateCbArgs const *args) -> GvoxResult {
            GvoxBoundedRawContainerConfig config;
            if (args->config != nullptr) {
                config = *static_cast<GvoxBoundedRawContainerConfig const *>(args->config);
            } else {
                config = {};
            }
            (*out_self) = new (std::nothrow) GvoxBoundedRawContainer({
                .voxel_desc = config.voxel_desc,
                .extent = config.extent,
                .pre_allocated_buffer = config.pre_allocated_buffer,
                .pre_alloc_offset_buffer = std::vector<int64_t>(static_cast<size_t>(config.extent.axis_n) * 3),
            });
            return GVOX_SUCCESS;
        },
        .destroy = [](void *self_ptr) { delete static_cast<GvoxBoundedRawContainer *>(self_ptr); },
        .fill = [](void *self_ptr, void const *single_voxel_data, GvoxVoxelDesc src_voxel_desc, GvoxRange range) -> GvoxResult {
            auto &self = *static_cast<GvoxBoundedRawContainer *>(self_ptr);

            // convert src data to be compatible with the dst_voxel_desc
            const auto *converted_data = static_cast<void const *>(nullptr);
            // test to see if the input data is already compatible (basically if it's the same exact voxel desc)
            auto converted_temp_storage = std::vector<uint8_t>{};
            auto self_voxel_size = (gvox_voxel_desc_size_in_bits(self.voxel_desc) + 7) >> 3;
            if (gvox_voxel_desc_compare(src_voxel_desc, self.voxel_desc) != 0) {
                converted_data = single_voxel_data;
            } else {
                converted_temp_storage.resize(self_voxel_size);
                // TODO: create standard mapping?
                auto mapping = GvoxAttributeMapping{.dst_index = 0, .src_index = 0};
                auto res = gvox_translate_voxel(single_voxel_data, src_voxel_desc, converted_temp_storage.data(), self.voxel_desc, &mapping, 1);
                if (res != GVOX_SUCCESS) {
                    return res;
                }
                converted_data = converted_temp_storage.data();
            }

            if (range.offset.axis_n < range.extent.axis_n) {
                return GVOX_ERROR_INVALID_ARGUMENT;
            }
            auto const dim = std::min(self.extent.axis_n, range.extent.axis_n);

            if (dim < 1) {
                return GVOX_SUCCESS;
            }

            bool is_single_voxel = true;
            for (uint32_t i = 0; i < dim; ++i) {
                if (range.extent.axis[i] == 0) {
                    return GVOX_SUCCESS;
                }
                if (range.extent.axis[i] != 1) {
                    is_single_voxel = false;
                }
            }

            Voxel const in_voxel = {
                .ptr = static_cast<uint8_t const *>(converted_data),
                .size = static_cast<uint32_t>(self_voxel_size),
            };

            auto &offset_buffer = self.pre_alloc_offset_buffer;
            auto voxel_range_offset = GvoxOffsetMut{.axis_n = dim, .axis = offset_buffer.data() + std::ptrdiff_t(0 * dim)};
            auto voxel_range_extent = GvoxExtentMut{.axis_n = dim, .axis = reinterpret_cast<uint64_t *>(offset_buffer.data() + std::ptrdiff_t(1 * dim))};
            auto voxel_next = GvoxOffsetMut{.axis_n = dim, .axis = offset_buffer.data() + std::ptrdiff_t(2 * dim)};

            auto *voxel_ptr = static_cast<uint8_t *>(self.pre_allocated_buffer);
            {
                auto stride = size_t{in_voxel.size};

                bool completely_clipped = false;
                for (uint32_t i = 0; i < dim; ++i) {
                    auto p0 = range.offset.axis[i];
                    auto p1 = p0 + static_cast<int64_t>(range.extent.axis[i]) - 1;
                    if (p0 >= static_cast<int64_t>(self.extent.axis[i]) || p1 < 0) {
                        // NOTE: Should be impossible
                        completely_clipped = true;
                        break;
                    }
                    if (p0 < 0) {
                        p0 = 0;
                    }
                    if (p1 >= static_cast<int64_t>(self.extent.axis[i])) {
                        p1 = static_cast<int64_t>(self.extent.axis[i]) - 1;
                    }
                    voxel_range_offset.axis[i] = p0;
                    voxel_range_extent.axis[i] = static_cast<uint64_t>(p1 - p0 + 1);

                    voxel_ptr += static_cast<size_t>(voxel_range_offset.axis[i]) * stride;
                    voxel_next.axis[i] = static_cast<int64_t>(static_cast<uint64_t>(stride) * (self.extent.axis[i] - voxel_range_extent.axis[i]));
                    stride *= self.extent.axis[i];
                }

                if (completely_clipped) {
                    return GVOX_SUCCESS;
                }
            }

            if (is_single_voxel) {
                set(voxel_ptr, in_voxel);
            } else {
                fill_Nd(dim, voxel_ptr, in_voxel, voxel_range_extent, voxel_next);
            }

            return GVOX_SUCCESS;
        },
        .move = [](void *, GvoxContainer *, GvoxRange *, GvoxOffset *, uint32_t) -> GvoxResult {
            return GVOX_ERROR_UNKNOWN;
        },
        .sample = [](void *self_ptr, GvoxSample const *samples, uint32_t sample_n) -> GvoxResult {
            for (uint32_t sample_i = 0; sample_i < sample_n; ++sample_i) {
                auto const &out_voxel_data = samples[sample_i].dst_voxel_data;
                auto const &out_voxel_desc = samples[sample_i].dst_voxel_desc;
                auto const &offset = samples[sample_i].offset;

                auto &self = *static_cast<GvoxBoundedRawContainer *>(self_ptr);
                auto const dim = std::min(offset.axis_n, self.extent.axis_n);

                auto voxel_offset = uint64_t{0};
                auto voxel_size_bytes = (gvox_voxel_desc_size_in_bits(self.voxel_desc) + 7) >> 3;
                auto stride = uint64_t{voxel_size_bytes};

                for (size_t i = 0; i < dim; ++i) {
                    if (offset.axis[i] < 0 || offset.axis[i] >= static_cast<int64_t>(self.extent.axis[i])) {
                        return GVOX_SUCCESS;
                    }
                    auto axis_p = static_cast<uint64_t>(offset.axis[i]);
                    voxel_offset += axis_p * stride;
                    stride *= self.extent.axis[i];
                }

                auto *data = static_cast<uint8_t *>(self.pre_allocated_buffer);
                // TODO: Actually fix this
                auto mapping = GvoxAttributeMapping{.dst_index = 0, .src_index = 0};
                auto res = gvox_translate_voxel(data + voxel_offset, self.voxel_desc, out_voxel_data, out_voxel_desc, &mapping, 1);
                if (res != GVOX_SUCCESS) {
                    return res;
                }
            }
            return GVOX_SUCCESS;
        },
    };
}
