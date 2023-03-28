/* SPDX-License-Identifier: Apache-2.0 */

#include "MEM_guardedalloc.h"

#include "BLI_implicit_sharing_ptr.hh"

#include "testing/testing.h"

namespace blender::tests {

class ImplicitlySharedData : public ImplicitSharingMixin {
 public:
  ImplicitSharingPtr<ImplicitlySharedData> copy() const
  {
    return MEM_new<ImplicitlySharedData>(__func__);
  }

  void delete_self() override
  {
    MEM_delete(this);
  }
};

class SharedDataContainer {
 private:
  ImplicitSharingPtr<ImplicitlySharedData> data_;

 public:
  SharedDataContainer() : data_(MEM_new<ImplicitlySharedData>(__func__))
  {
  }

  const ImplicitlySharedData *get_for_read() const
  {
    return data_.get();
  }

  ImplicitlySharedData *get_for_write()
  {
    if (!data_) {
      return nullptr;
    }
    if (data_->is_mutable()) {
      return data_.get();
    }
    data_ = data_->copy();
    return data_.get();
  }
};

TEST(implicit_sharing, CopyOnWriteAccess)
{
  /* Create the initial data. */
  SharedDataContainer a;
  EXPECT_NE(a.get_for_read(), nullptr);

  /* a and b share the same underlying data now. */
  SharedDataContainer b = a;
  EXPECT_EQ(a.get_for_read(), b.get_for_read());

  /* c now shares the data with a and b. */
  SharedDataContainer c = a;
  EXPECT_EQ(b.get_for_read(), c.get_for_read());

  /* Retrieving write access on b should make a copy because the data is shared. */
  ImplicitlySharedData *data_b1 = b.get_for_write();
  EXPECT_NE(data_b1, nullptr);
  EXPECT_EQ(data_b1, b.get_for_read());
  EXPECT_NE(data_b1, a.get_for_read());
  EXPECT_NE(data_b1, c.get_for_read());

  /* Retrieving the same write access again should *not* make another copy. */
  ImplicitlySharedData *data_b2 = b.get_for_write();
  EXPECT_EQ(data_b1, data_b2);

  /* Moving b should also move the data. b then does not have ownership anymore. Since the data in
   * b only had one owner, the data is still mutable now that d is the owner. */
  SharedDataContainer d = std::move(b);
  EXPECT_EQ(b.get_for_read(), nullptr);
  EXPECT_EQ(b.get_for_write(), nullptr);
  EXPECT_EQ(d.get_for_read(), data_b1);
  EXPECT_EQ(d.get_for_write(), data_b1);
}

}  // namespace blender::tests
