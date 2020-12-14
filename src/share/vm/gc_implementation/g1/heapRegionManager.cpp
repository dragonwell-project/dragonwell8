/*
 * Copyright (c) 2001, 2015, Oracle and/or its affiliates. All rights reserved.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 * This code is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 only, as
 * published by the Free Software Foundation.
 *
 * This code is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * version 2 for more details (a copy is included in the LICENSE file that
 * accompanied this code).
 *
 * You should have received a copy of the GNU General Public License version
 * 2 along with this work; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * Please contact Oracle, 500 Oracle Parkway, Redwood Shores, CA 94065 USA
 * or visit www.oracle.com if you need additional information or have any
 * questions.
 *
 */

#include "precompiled.hpp"
#include "gc_implementation/g1/heapRegion.hpp"
#include "gc_implementation/g1/heapRegionManager.inline.hpp"
#include "gc_implementation/g1/heapRegionSet.inline.hpp"
#include "gc_implementation/g1/g1CollectedHeap.inline.hpp"
#include "gc_implementation/g1/concurrentG1Refine.hpp"
#include "memory/allocation.hpp"
#include "runtime/os.hpp"
#include "gc_implementation/g1/elasticHeap.hpp"

void HeapRegionManager::initialize(G1RegionToSpaceMapper* heap_storage,
                               G1RegionToSpaceMapper* prev_bitmap,
                               G1RegionToSpaceMapper* next_bitmap,
                               G1RegionToSpaceMapper* bot,
                               G1RegionToSpaceMapper* cardtable,
                               G1RegionToSpaceMapper* card_counts) {
  _allocated_heapregions_length = 0;

  _heap_mapper = heap_storage;

  _prev_bitmap_mapper = prev_bitmap;
  _next_bitmap_mapper = next_bitmap;

  _bot_mapper = bot;
  _cardtable_mapper = cardtable;

  _card_counts_mapper = card_counts;

  MemRegion reserved = heap_storage->reserved();
  _regions.initialize(reserved.start(), reserved.end(), HeapRegion::GrainBytes);

  _available_map.resize(_regions.length(), false);
  _available_map.clear();
}

bool HeapRegionManager::is_available(uint region) const {
  return _available_map.at(region);
}

#ifdef ASSERT
bool HeapRegionManager::is_free(HeapRegion* hr) const {
  return _free_list.contains(hr);
}
#endif

HeapRegion* HeapRegionManager::new_heap_region(uint hrm_index) {
  G1CollectedHeap* g1h = G1CollectedHeap::heap();
  HeapWord* bottom = g1h->bottom_addr_for_region(hrm_index);
  MemRegion mr(bottom, bottom + HeapRegion::GrainWords);
  assert(reserved().contains(mr), "invariant");
  return g1h->allocator()->new_heap_region(hrm_index, g1h->bot_shared(), mr);
}

void HeapRegionManager::commit_regions(uint index, size_t num_regions) {
  guarantee(num_regions > 0, "Must commit more than zero regions");
  guarantee(_num_committed + num_regions <= max_length(), "Cannot commit more than the maximum amount of regions");

  _num_committed += (uint)num_regions;

  _heap_mapper->commit_regions(index, num_regions);

  // Also commit auxiliary data
  _prev_bitmap_mapper->commit_regions(index, num_regions);
  _next_bitmap_mapper->commit_regions(index, num_regions);

  _bot_mapper->commit_regions(index, num_regions);
  _cardtable_mapper->commit_regions(index, num_regions);

  _card_counts_mapper->commit_regions(index, num_regions);
}

void HeapRegionManager::uncommit_region_memory(uint idx) {
  assert(G1ElasticHeap, "Precondition");
  // Print before uncommitting.
  if (G1CollectedHeap::heap()->hr_printer()->is_active()) {
    HeapRegion* hr = at(idx);
    G1CollectedHeap::heap()->hr_printer()->uncommit(hr->bottom(), hr->end());
  }

  _heap_mapper->par_uncommit_region_memory(idx);
}

void HeapRegionManager::commit_region_memory(uint idx) {
  assert(G1ElasticHeap, "Precondition");
  // Print before committing.
  if (G1CollectedHeap::heap()->hr_printer()->is_active()) {
    HeapRegion* hr = at(idx);
    G1CollectedHeap::heap()->hr_printer()->commit(hr->bottom(), hr->end());
  }

  _heap_mapper->par_commit_region_memory(idx);

  // Only the bitmap needs on-commit operations for free regions
  // It's safe to clear bitmap range of a region parallelly
  _prev_bitmap_mapper->fire_on_commit(idx, 1, false);
  _next_bitmap_mapper->fire_on_commit(idx, 1, false);
}

void HeapRegionManager::free_region_memory(uint idx) {
  assert(G1ElasticHeap, "Precondition");
  _heap_mapper->free_region_memory(idx);
}

void HeapRegionManager::uncommit_regions(uint start, size_t num_regions) {
  guarantee(num_regions >= 1, err_msg("Need to specify at least one region to uncommit, tried to uncommit zero regions at %u", start));
  guarantee(_num_committed >= num_regions, "pre-condition");

  // Print before uncommitting.
  if (G1CollectedHeap::heap()->hr_printer()->is_active()) {
    for (uint i = start; i < start + num_regions; i++) {
      HeapRegion* hr = at(i);
      G1CollectedHeap::heap()->hr_printer()->uncommit(hr->bottom(), hr->end());
    }
  }

  _num_committed -= (uint)num_regions;

  _available_map.par_clear_range(start, start + num_regions, BitMap::unknown_range);
  _heap_mapper->uncommit_regions(start, num_regions);

  // Also uncommit auxiliary data
  _prev_bitmap_mapper->uncommit_regions(start, num_regions);
  _next_bitmap_mapper->uncommit_regions(start, num_regions);

  _bot_mapper->uncommit_regions(start, num_regions);
  _cardtable_mapper->uncommit_regions(start, num_regions);

  _card_counts_mapper->uncommit_regions(start, num_regions);
}

void HeapRegionManager::make_regions_available(uint start, uint num_regions) {
  guarantee(num_regions > 0, "No point in calling this for zero regions");
  commit_regions(start, num_regions);
  for (uint i = start; i < start + num_regions; i++) {
    if (_regions.get_by_index(i) == NULL) {
      HeapRegion* new_hr = new_heap_region(i);
      _regions.set_by_index(i, new_hr);
      _allocated_heapregions_length = MAX2(_allocated_heapregions_length, i + 1);
    }
  }

  _available_map.par_set_range(start, start + num_regions, BitMap::unknown_range);

  for (uint i = start; i < start + num_regions; i++) {
    assert(is_available(i), err_msg("Just made region %u available but is apparently not.", i));
    HeapRegion* hr = at(i);
    if (G1CollectedHeap::heap()->hr_printer()->is_active()) {
      G1CollectedHeap::heap()->hr_printer()->commit(hr->bottom(), hr->end());
    }
    HeapWord* bottom = G1CollectedHeap::heap()->bottom_addr_for_region(i);
    MemRegion mr(bottom, bottom + HeapRegion::GrainWords);

    hr->initialize(mr);
    insert_into_free_list(at(i));
  }
}

MemoryUsage HeapRegionManager::get_auxiliary_data_memory_usage() const {
  size_t used_sz =
    _prev_bitmap_mapper->committed_size() +
    _next_bitmap_mapper->committed_size() +
    _bot_mapper->committed_size() +
    _cardtable_mapper->committed_size() +
    _card_counts_mapper->committed_size();

  size_t committed_sz =
    _prev_bitmap_mapper->reserved_size() +
    _next_bitmap_mapper->reserved_size() +
    _bot_mapper->reserved_size() +
    _cardtable_mapper->reserved_size() +
    _card_counts_mapper->reserved_size();

  return MemoryUsage(0, used_sz, committed_sz, committed_sz);
}

uint HeapRegionManager::expand_by(uint num_regions) {
  return expand_at(0, num_regions);
}

uint HeapRegionManager::expand_at(uint start, uint num_regions) {
  if (num_regions == 0) {
    return 0;
  }

  if (G1ElasticHeap && G1CollectedHeap::heap()->elastic_heap() != NULL) {
    // Heap initialization completes. Don't expand ever.
    return 0;
  }

  uint cur = start;
  uint idx_last_found = 0;
  uint num_last_found = 0;

  uint expanded = 0;

  while (expanded < num_regions &&
         (num_last_found = find_unavailable_from_idx(cur, &idx_last_found)) > 0) {
    uint to_expand = MIN2(num_regions - expanded, num_last_found);
    make_regions_available(idx_last_found, to_expand);
    expanded += to_expand;
    cur = idx_last_found + num_last_found + 1;
  }

  verify_optional();
  return expanded;
}

uint HeapRegionManager::find_contiguous(size_t num, bool empty_only) {
  uint found = 0;
  size_t length_found = 0;
  uint cur = 0;

  while (length_found < num && cur < max_length()) {
    HeapRegion* hr = _regions.get_by_index(cur);
    if ((!empty_only && !is_available(cur) && !G1ElasticHeap) || (is_available(cur) && hr != NULL && hr->is_empty())) {
      // This region is a potential candidate for allocation into.
      length_found++;
    } else {
      // This region is not a candidate. The next region is the next possible one.
      found = cur + 1;
      length_found = 0;
    }
    cur++;
  }

  if (length_found == num) {
    for (uint i = found; i < (found + num); i++) {
      HeapRegion* hr = _regions.get_by_index(i);
      // sanity check
      guarantee((!empty_only && !is_available(i)) || (is_available(i) && hr != NULL && hr->is_empty()),
                err_msg("Found region sequence starting at " UINT32_FORMAT ", length " SIZE_FORMAT
                        " that is not empty at " UINT32_FORMAT ". Hr is " PTR_FORMAT, found, num, i, p2i(hr)));
    }
    return found;
  } else {
    return G1_NO_HRM_INDEX;
  }
}

HeapRegion* HeapRegionManager::next_region_in_heap(const HeapRegion* r) const {
  guarantee(r != NULL, "Start region must be a valid region");
  guarantee(is_available(r->hrm_index()), err_msg("Trying to iterate starting from region %u which is not in the heap", r->hrm_index()));
  for (uint i = r->hrm_index() + 1; i < _allocated_heapregions_length; i++) {
    HeapRegion* hr = _regions.get_by_index(i);
    if (is_available(i)) {
      return hr;
    }
  }
  return NULL;
}

void HeapRegionManager::iterate(HeapRegionClosure* blk) const {
  uint len = max_length();

  for (uint i = 0; i < len; i++) {
    if (!is_available(i)) {
      continue;
    }
    guarantee(at(i) != NULL, err_msg("Tried to access region %u that has a NULL HeapRegion*", i));
    bool res = blk->doHeapRegion(at(i));
    if (res) {
      blk->incomplete();
      return;
    }
  }
}

uint HeapRegionManager::find_unavailable_from_idx(uint start_idx, uint* res_idx) const {
  guarantee(res_idx != NULL, "checking");
  guarantee(start_idx <= (max_length() + 1), "checking");

  uint num_regions = 0;

  uint cur = start_idx;
  while (cur < max_length() && is_available(cur)) {
    cur++;
  }
  if (cur == max_length()) {
    return num_regions;
  }
  *res_idx = cur;
  while (cur < max_length() && !is_available(cur)) {
    cur++;
  }
  num_regions = cur - *res_idx;
#ifdef ASSERT
  for (uint i = *res_idx; i < (*res_idx + num_regions); i++) {
    assert(!is_available(i), "just checking");
  }
  assert(cur == max_length() || num_regions == 0 || is_available(cur),
         err_msg("The region at the current position %u must be available or at the end of the heap.", cur));
#endif
  return num_regions;
}

uint HeapRegionManager::start_region_for_worker(uint worker_i, uint num_workers, uint num_regions) const {
  return num_regions * worker_i / num_workers;
}

void HeapRegionManager::par_iterate(HeapRegionClosure* blk, uint worker_id, uint num_workers, jint claim_value) const {
  const uint start_index = start_region_for_worker(worker_id, num_workers, _allocated_heapregions_length);

  // Every worker will actually look at all regions, skipping over regions that
  // are currently not committed.
  // This also (potentially) iterates over regions newly allocated during GC. This
  // is no problem except for some extra work.
  for (uint count = 0; count < _allocated_heapregions_length; count++) {
    const uint index = (start_index + count) % _allocated_heapregions_length;
    assert(0 <= index && index < _allocated_heapregions_length, "sanity");
    // Skip over unavailable regions
    if (!is_available(index)) {
      continue;
    }
    HeapRegion* r = _regions.get_by_index(index);
    // We'll ignore "continues humongous" regions (we'll process them
    // when we come across their corresponding "start humongous"
    // region) and regions already claimed.
    if (r->claim_value() == claim_value || r->continuesHumongous()) {
      continue;
    }
    // OK, try to claim it
    if (!r->claimHeapRegion(claim_value)) {
      continue;
    }
    // Success!
    if (r->startsHumongous()) {
      // If the region is "starts humongous" we'll iterate over its
      // "continues humongous" first; in fact we'll do them
      // first. The order is important. In one case, calling the
      // closure on the "starts humongous" region might de-allocate
      // and clear all its "continues humongous" regions and, as a
      // result, we might end up processing them twice. So, we'll do
      // them first (note: most closures will ignore them anyway) and
      // then we'll do the "starts humongous" region.
      for (uint ch_index = index + 1; ch_index < index + r->region_num(); ch_index++) {
        HeapRegion* chr = _regions.get_by_index(ch_index);

        assert(chr->continuesHumongous(), "Must be humongous region");
        assert(chr->humongous_start_region() == r,
               err_msg("Must work on humongous continuation of the original start region "
                       PTR_FORMAT ", but is " PTR_FORMAT, p2i(r), p2i(chr)));
        assert(chr->claim_value() != claim_value,
               "Must not have been claimed yet because claiming of humongous continuation first claims the start region");

        bool claim_result = chr->claimHeapRegion(claim_value);
        // We should always be able to claim it; no one else should
        // be trying to claim this region.
        guarantee(claim_result, "We should always be able to claim the continuesHumongous part of the humongous object");

        bool res2 = blk->doHeapRegion(chr);
        if (res2) {
          return;
        }

        // Right now, this holds (i.e., no closure that actually
        // does something with "continues humongous" regions
        // clears them). We might have to weaken it in the future,
        // but let's leave these two asserts here for extra safety.
        assert(chr->continuesHumongous(), "should still be the case");
        assert(chr->humongous_start_region() == r, "sanity");
      }
    }

    bool res = blk->doHeapRegion(r);
    if (res) {
      return;
    }
  }
}

void HeapRegionManager::commit_region_memory(FreeRegionList* list) {
  assert(G1ElasticHeap, "Precondition");

  FreeRegionListIterator iter(list);
  while (iter.more_available()) {
    HeapRegion* hr = iter.get_next();
    commit_region_memory(hr->hrm_index());
  }
}

void HeapRegionManager::uncommit_region_memory(FreeRegionList* list) {
  assert(G1ElasticHeap, "Precondition");

  FreeRegionListIterator iter(list);
  while (iter.more_available()) {
    HeapRegion* hr = iter.get_next();
    uncommit_region_memory(hr->hrm_index());
  }
}

void HeapRegionManager::set_region_available(FreeRegionList* list) {
  assert(G1ElasticHeap, "Precondition");
  FreeRegionListIterator iter(list);
  while (iter.more_available()) {
    HeapRegion* hr = iter.get_next();
    _available_map.par_set_range(hr->hrm_index(), hr->hrm_index() + 1, BitMap::unknown_range);
  }
  _num_committed += list->length();
}

void HeapRegionManager::set_region_unavailable(FreeRegionList* list) {
  assert(G1ElasticHeap, "Precondition");
  FreeRegionListIterator iter(list);
  while (iter.more_available()) {
    HeapRegion* hr = iter.get_next();
    _available_map.par_clear_range(hr->hrm_index(), hr->hrm_index() + 1, BitMap::unknown_range);
  }
  assert(_num_committed > list->length(), "sanity");
  _num_committed -= list->length();
}

uint HeapRegionManager::num_uncommitted_regions() {
  assert(G1ElasticHeap, "Precondition");
  return _uncommitted_list.length();
}

void HeapRegionManager::recover_uncommitted_regions() {
  assert(G1ElasticHeap, "Precondition");
  assert_at_safepoint(true /* should_be_vm_thread */);

  commit_region_memory(&_uncommitted_list);
  set_region_available(&_uncommitted_list);
  _free_list.add_ordered(&_uncommitted_list);
}

void HeapRegionManager::move_to_uncommitted_list(FreeRegionList* list) {
  assert(G1ElasticHeap, "Precondition");
  assert_at_safepoint(true /* should_be_vm_thread */);

  _uncommitted_list.add_ordered(list);
}

void HeapRegionManager::move_to_free_list(FreeRegionList* list) {
  assert(G1ElasticHeap, "Precondition");
  assert_at_safepoint(true /* should_be_vm_thread */);

  set_region_available(list);
  _free_list.add_ordered(list);
}

void HeapRegionManager::prepare_uncommit_regions(FreeRegionList* list, uint num) {
  assert(G1ElasticHeap, "Precondition");
  assert(num <= _free_list.length(), "sanity");
  assert_at_safepoint(true /* should_be_vm_thread */);
  assert(list->is_empty(), "sanity");

  G1CollectedHeap* g1h = G1CollectedHeap::heap();
  for (uint i = 0; i < num; i++) {
    HeapRegion* hr = _free_list.remove_region(false /* from_head */);
    list->add_ordered(hr);
  }
  set_region_unavailable(list);
}

void HeapRegionManager::prepare_commit_regions(FreeRegionList* list, uint num) {
  assert(G1ElasticHeap, "Precondition");
  assert(num <= _uncommitted_list.length(), "sanity");
  assert_at_safepoint(true /* should_be_vm_thread */);
  assert(list->is_empty(), "sanity");

  G1CollectedHeap* g1h = G1CollectedHeap::heap();
  for (uint i = 0; i < num; i++) {
    HeapRegion* hr = _uncommitted_list.remove_region(true /* from_head */);
    assert(!is_available(hr->hrm_index()), "sanity");
    list->add_ordered(hr);
  }
}

void HeapRegionManager::prepare_old_region_list_to_free(FreeRegionList* to_free_list,
                                                        uint reserve_regions,
                                                        uint free_regions_for_young_gen) {
  assert(G1ElasticHeap, "Precondition");
  assert_at_safepoint(true /* should_be_vm_thread */);
  assert(to_free_list->is_empty(), "sanity");

  G1CollectedHeap* g1h = G1CollectedHeap::heap();
  assert(!g1h->elastic_heap()->in_conc_cycle(), "Precondition");

  if (_free_list.length() <= (reserve_regions + free_regions_for_young_gen)) {
    // Not enough free regions
    return;
  }

  uint uncommit_regions = _free_list.length() - reserve_regions - free_regions_for_young_gen;
  FreeRegionListIterator iter(&_free_list);
  // Skip reserve regions from the head of _free_list
  for (uint i = 0; i < reserve_regions; i++) {
    assert(iter.more_available(), "sanity");
    iter.get_next();
  }
  // Remove regions out of free list for uncommit
  for (uint i = 0; i < uncommit_regions; i++) {
    assert(iter.more_available(), "sanity");
    HeapRegion* hr = iter.remove_next();
    to_free_list->add_ordered(hr);
  }
  set_region_unavailable(to_free_list);
}

uint HeapRegionManager::shrink_by(uint num_regions_to_remove) {
  assert(length() > 0, "the region sequence should not be empty");
  assert(length() <= _allocated_heapregions_length, "invariant");
  assert(_allocated_heapregions_length > 0, "we should have at least one region committed");
  assert(num_regions_to_remove < length(), "We should never remove all regions");

  if (num_regions_to_remove == 0) {
    return 0;
  }

  uint removed = 0;
  uint cur = _allocated_heapregions_length - 1;
  uint idx_last_found = 0;
  uint num_last_found = 0;

  while ((removed < num_regions_to_remove) &&
      (num_last_found = find_empty_from_idx_reverse(cur, &idx_last_found)) > 0) {
    uint to_remove = MIN2(num_regions_to_remove - removed, num_last_found);

    uncommit_regions(idx_last_found + num_last_found - to_remove, to_remove);

    cur -= num_last_found;
    removed += to_remove;
  }

  verify_optional();

  return removed;
}

uint HeapRegionManager::find_empty_from_idx_reverse(uint start_idx, uint* res_idx) const {
  guarantee(start_idx < _allocated_heapregions_length, "checking");
  guarantee(res_idx != NULL, "checking");

  uint num_regions_found = 0;

  jlong cur = start_idx;
  while (cur != -1 && !(is_available(cur) && at(cur)->is_empty())) {
    cur--;
  }
  if (cur == -1) {
    return num_regions_found;
  }
  jlong old_cur = cur;
  // cur indexes the first empty region
  while (cur != -1 && is_available(cur) && at(cur)->is_empty()) {
    cur--;
  }
  *res_idx = cur + 1;
  num_regions_found = old_cur - cur;

#ifdef ASSERT
  for (uint i = *res_idx; i < (*res_idx + num_regions_found); i++) {
    assert(at(i)->is_empty(), "just checking");
  }
#endif
  return num_regions_found;
}

void HeapRegionManager::verify() {
  guarantee(length() <= _allocated_heapregions_length,
            err_msg("invariant: _length: %u _allocated_length: %u",
                    length(), _allocated_heapregions_length));
  guarantee(_allocated_heapregions_length <= max_length(),
            err_msg("invariant: _allocated_length: %u _max_length: %u",
                    _allocated_heapregions_length, max_length()));

  bool prev_committed = true;
  uint num_committed = 0;
  HeapWord* prev_end = heap_bottom();
  for (uint i = 0; i < _allocated_heapregions_length; i++) {
    if (!is_available(i)) {
      prev_committed = false;
      continue;
    }
    num_committed++;
    HeapRegion* hr = _regions.get_by_index(i);
    guarantee(hr != NULL, err_msg("invariant: i: %u", i));
    guarantee(!prev_committed || hr->bottom() == prev_end,
              err_msg("invariant i: %u " HR_FORMAT " prev_end: " PTR_FORMAT,
                      i, HR_FORMAT_PARAMS(hr), p2i(prev_end)));
    guarantee(hr->hrm_index() == i,
              err_msg("invariant: i: %u hrm_index(): %u", i, hr->hrm_index()));
    // Asserts will fire if i is >= _length
    HeapWord* addr = hr->bottom();
    guarantee(addr_to_region(addr) == hr, "sanity");
    // We cannot check whether the region is part of a particular set: at the time
    // this method may be called, we have only completed allocation of the regions,
    // but not put into a region set.
    prev_committed = true;
    if (hr->startsHumongous()) {
      prev_end = hr->orig_end();
    } else {
      prev_end = hr->end();
    }
  }
  for (uint i = _allocated_heapregions_length; i < max_length(); i++) {
    guarantee(_regions.get_by_index(i) == NULL, err_msg("invariant i: %u", i));
  }

  guarantee(num_committed == _num_committed, err_msg("Found %u committed regions, but should be %u", num_committed, _num_committed));
  _free_list.verify();
}

#ifndef PRODUCT
void HeapRegionManager::verify_optional() {
  verify();
}
#endif // PRODUCT

