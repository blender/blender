/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#include "spreadsheet_cache.hh"

namespace blender::ed::spreadsheet {

void SpreadsheetCache::add(std::unique_ptr<Key> key, std::unique_ptr<Value> value)
{
  key->is_used = true;
  cache_map_.add_overwrite(*key, std::move(value));
  keys_.append(std::move(key));
}

SpreadsheetCache::Value *SpreadsheetCache::lookup(const Key &key)
{
  std::unique_ptr<Value> *value = cache_map_.lookup_ptr(key);
  if (value == nullptr) {
    return nullptr;
  }
  const Key &stored_cache_key = cache_map_.lookup_key(key);
  stored_cache_key.is_used = true;
  return value->get();
}

SpreadsheetCache::Value &SpreadsheetCache::lookup_or_add(
    std::unique_ptr<Key> key, FunctionRef<std::unique_ptr<Value>()> create_value)
{
  Value *value = this->lookup(*key);
  if (value != nullptr) {
    return *value;
  }
  std::unique_ptr<Value> new_value = create_value();
  value = new_value.get();
  this->add(std::move(key), std::move(new_value));
  return *value;
}

void SpreadsheetCache::set_all_unused()
{
  for (std::unique_ptr<Key> &key : keys_) {
    key->is_used = false;
  }
}

void SpreadsheetCache::remove_all_unused()
{
  /* First remove the keys from the map and free the values. */
  for (auto it = cache_map_.keys().begin(); it != cache_map_.keys().end(); ++it) {
    const Key &key = *it;
    if (!key.is_used) {
      cache_map_.remove(it);
    }
  }
  /* Then free the keys. */
  for (int i = 0; i < keys_.size();) {
    if (keys_[i]->is_used) {
      i++;
    }
    else {
      keys_.remove_and_reorder(i);
    }
  }
}

}  // namespace blender::ed::spreadsheet
