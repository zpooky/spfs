#include "btree.h"

//=====================================
enum ChildDir { ChildDir_LEFT, ChildDir_RIGHT };

static bool
is_leaf(const struct spfs_bnode *node) {
  size_t i;
  size_t len = bnode_length(node) + 1;

  for (i = 0; i < len; ++i) {
    sector_t child;
    child = bnode_get_child(node, i);
    if (child != 0) {
      return false;
    }
  }

  return true;
}

static size_t
order(const struct spfs_bnode *node) {
  return bnode_capacity(node) + 1;
}

static size_t
minimum_order(const struct spfs_bnode *node) {
  return order(node) / 2;
}

static bool
is_deficient(const struct spfs_bnode *node) {
  return bnode_length(node) < minimum_order(node);
}

static bool
has_more_than_min(const struct spfs_bnode *node) {
  return bnode_length(node) > minimum_order(node);
}

static size_t
remaining_write(const struct spfs_bnode *node) {
  return bnode_capacity(node) - bnode_length(node);
}

static bool
move_all_elements(struct spfs_bnode *dest, struct spfs_bnode *src) {

  // TODO
  return false;
}

static bool
insert_all_children(struct spfs_bnode *dest, struct spfs_bnode *src) {

  // TODO
  return false;
}

static bool
merge(struct spfs_bnode *dest, struct spfs_bnode *src) {
  bool result;

  BUG_ON(!src);
  BUG_ON(!dest);

  if (bnode_length(src) > remaining_write(dest)) {
    return false;
  }

  result = move_all_elements(dest, src);
  if (result) {
    bool res;
    res = insert_all_children(dest, src);
    BUG_ON(!res);

    /* TODO delete src; */
  }

  return result;
}

static bool
bnode_stable_take_elem(struct spfs_btree *self, struct spfs_bnode *node,
                       size_t index, struct spfs_bentry *out) {
  // TODO
  return false;
}

static bool
bnode_stable_remove_child(struct spfs_btree *self, struct spfs_bnode *node,
                          size_t index) {
  // TODO
  return false;
}

static sector_t
bnode_greatest_child(struct spfs_btree *self, struct spfs_bnode *node) {
  // AKA last
  // TODO
  return 0;
}

static struct spfs_bentry *
bnode_bin_insert(struct spfs_btree *self, struct spfs_bnode *dest,
                 sector_t less, struct spfs_bentry *value) noexcept {
  /* auto &children = dest.children; */
  /* auto &elements = dest.elements; */
  /*  */
  /* Cmp cmp; */
  /* T *const result = bin_insert(elements, std::forward<Key>(value), cmp); */
  /* assertxs(result, length(elements), capacity(elements), length(children), */
  /*          capacity(children)); */
  /*  */
  /* const std::size_t index = index_of(elements, result); */
  /* assertxs(index != capacity(elements), index, length(elements), */
  /*          capacity(elements)); */
  /*  */
  /* { */
  /*   BTNode<T, keys, Cmp> **const l = insert_at(children, index, less); */
  /*   assertx(l); */
  /*   assertx(*l == less); */
  /* } */
  /*  */
  /* if (length(children) == 1) { */
  /*   assertx(length(elements) == 1); */
  /*  */
  /*   BTNode<T, keys, Cmp> *const greater = nullptr; */
  /*   auto res = insert(children, greater); */
  /*   assertx(res); */
  /*   assertx(*res == greater); */
  /* } */
  /*  */
  /* return result; */
}

static bool
rebalance(struct spfs_btree *self, struct spfs_bnode *parent,
          struct spfs_bentry *pivot, ChildDir dir) {
  BUG_ON(is_leaf(parent));
  BUG_ON(!pivot);

  /* auto &elements = parent.elements; */
  /* auto &children = parent.children; */

  const size_t piv_idx = bnode_index_of(self, parent, pivot);
  BUG_ON(piv_idx == capacity(elements));

  const size_t left_idx = piv_idx;
  const size_t right_idx = piv_idx + 1;

  {
    if (dir == ChildDir_LEFT) {
      /* # Rotation
       * before:
       *          [xxxx,pivot,yyyy]
       *         /     |     |     \
       *        ?     left  right   ?
       *               |     |
       *  [aa, nil, nil]     [bb, cc, dd]
       *  |   \             /   |    \   \
       * a0   a1          b0   b1    c1  d1
       *
       * after:
       *          [xxxx, bb ,yyyy]
       *         /     |    |     \
       *        ?     left right   ?
       *               |    |
       *  [aa,pivot,nil]    [cc, dd, nil]
       *  |  |     |       /   |    \
       * a0  a1    b0      b1  c1    d1
       *
       *
       * left of pivot is deficient:
       * 1. Copy the separator from the parent to the end of the deficient node
       *    (the separator moves down; the deficient node now has the minimum
       *    number of elements)
       * 2. Replace the separator in the parent with the first element of the
       *    right sibling (right sibling loses one node but still has at least
       *    the minimum number of elements)
       * 3. The tree is now balanced
       */

      const size_t subject_idx = piv_idx;
      sector_t subject = bnode_get_child(parent, subject_idx);
      sector_t right_sibling = bnode_get_child(parent, right_idx);
      BUG_ON(subject == 0);

      if (right_sibling) {
        if (has_more_than_min(right_sibling)) {

          // 1.
          {
            sector_t right_smallest = bnode_get_child(right_sibling, 0);
            struct spfs_bentry *res =
                bin_insert(self, subject, pivot, right_smallest);
            BUG_ON(!res);
          }

          // 2.
          {
            bool tres = bnode_stable_take_elem(right_sibling, 0, pivot);
            assertx(tres);

            bool res = bnode_stable_remove_child(right_sibling, 0);
            assertx(res);
          }

          // 3. done with rebalance
          return false;
        }
      }

    } /*ChildDir_RIGHT*/ else {
      /* # Rotation:
       * before:
       *          [xxxx,pivot,yyyy]
       *         /     |     |     \
       *        ?     left  right   ?
       *               |     |
       *   [aa ,bb , cc]     [dd, nil, nil]
       *  /   /    |   |     |   \
       * a0  a1    b1  c1    d0   d1
       *
       * after:
       *          [xxxx, cc ,yyyy]
       *         /     |    |     \
       *        ?     left right   ?
       *               |    |
       *   [aa ,bb ,nil]    [pivot, dd, nil]
       *  /   /    |        |     |    \
       * a0  a1    b1       c1    d0    d1
       *
       * right of pivot is deficient
       *
       * 1. Copy the separator from the parent to the start of the deficient
       *    node (the separator moves down; deficient node now has the minimum
       *    number of elements)
       * 2. Replace the separator in the parent with the last element of the
       *    left sibling (left sibling loses one node but still has at least
       *    the minimum number of elements)
       * 3. The tree is now balanced
       */

      const size_t subject_idx = piv_idx + 1;
      sector_t subject = bnode_get_child(parent, subject_idx);
      sector_t left_sibling = bnode_get_child(parent, left_idx);
      BUG_ON(!subject);

      if (left_sibling) {
        if (has_more_than_min(left_sibling)) {

          // 1.
          {
            sector_t left_greatest = bnode_greatest_child(self, left_sibling);
            BUG_ON(!left_greatest);

            struct spfs_bentry *res =
                bnode_bin_insert(self, subject, left_greatest, pivot);
            BUG_ON(!res);
          }

          // 2.
          {
            //<<===
            {
              size_t last_idx = bnode_length(left_sibling) - 1;
              bool res =
                  bnode_stable_take_elem(self, left_sibling, last_idx, pivot);
              assertx(res);
            }

            size_t last_idx = length(left_sibling->children) - 1;
            bool res = stable_remove(left_sibling->children, last_idx);
            assertx(res);
          }

          // 3.done with rebalance
          return false;
        }
      }
    }
  }

  /* # Merge
   * before:
   *        [xx  ,pivot,  yy]
   *       /     |     |     \
   *      x0    left  right   y0
   *             |     |
   * [aa ,nil,nil]     [dd, nil, nil]
   * |    \            |   \
   * a0    a1          d0   d1
   *
   * after:
   *   [xx  , yy]
   *  /     |    \
   * x0     |     y0
   *        |
   *        [aa,pivot,dd]
   *       /   |     |   \
   *      a0   a1    d0   d1
   *
   * Otherwise, if both immediate siblings have only the minimum number of
   * elements, then merge with a sibling sandwiching their separator taken
   * off from their parent

   * 1. Copy the separator to the end of the left node (the left node may be
   *    the deficient node or it may be the sibling with the minimum number
   *    of elements)
   * 2. Move all elements from the right node to the left node (the left node
   *    now has the maximum number of elements, and the right node – empty)
   * 3. Remove the separator from the parent along with its empty right child
   *    (the parent loses an element)
   *    - If the parent is the root and now has no elements, then free it and
   *      make the merged node the new root (tree becomes shallower)
   *    - Otherwise, if the parent has fewer than the required number of
   *      elements, then rebalance the parent
   */

  BTNode<T, keys, Cmp> *const left_child = children[left_idx];
  BTNode<T, keys, Cmp> *const right_child = children[right_idx];
  {
    if (!left_child && !right_child) {
      assertx(false);
      return false;
    }

    bool cont = false;
    if (right_child && left_child) {
      // TODO define behaviour
      if (!is_deficient(*left_child) && !is_deficient(*right_child)) {
        return false;
      }

      const size_t req_len = (length(right_child->elements) + 1);
      if (remaining_write(left_child->elements) >= req_len) {
        cont = true;
      }
    }

    if (!cont) {
      if (left_child && is_empty(left_child->elements)) {
        /*delete left child node if empy*/
        assertx(is_leaf(*left_child));
        { /**/
          delete left_child;
        }
        children[left_idx] = nullptr;
      }

      /*delete right child node if empy*/
      if (right_child && is_empty(right_child->elements)) {
        assertx(is_leaf(*right_child));
        { /**/
          delete right_child;
        }
        children[right_idx] = nullptr;
      }
      return false;
    }
  }

  // 1. move pivot into left
  {
    T *const res = insert(left_child->elements, std::move(*pivot));
    assertx(res);
    /* We explicitly do not add $gt child here since we will merge all children
     * from $right_child which is the old $gt of $pivot
     */
  }
  // 2. merge right into left and gc right
  {
    bool res = merge(/*DEST*/ *left_child, /*SRC->gc*/ right_child);
    assertx(res);
  }
  // 3. remove old pivot
  {
    {
      bool res = stable_remove(elements, piv_idx);
      assertx(res);
    }

    {
      bool res = stable_remove(children, piv_idx + 1);
      assertx(res);
    }
  }

  // continue rebalance
  return true;
}

static bool
take_wrapper(BTNode<T, keys, Cmp> &, T *, Dest &dest);

static bool
take_leaf(BTNode<T, keys, Cmp> &self, T *subject, Dest &dest) {
  assertx(is_leaf(self));

  auto &elements = self.elements;
  auto &children = self.children;

  const size_t index = index_of(elements, subject);
  assertxs(index != capacity(elements), index, length(elements));
  {
    bool res = stable_take(elements, index, dest);
    assertx(res);
  }
  {
    bool res = stable_remove(children, index + 1);
    assertx(res);
  }

  return is_deficient(self);
}

static bool
take_max(BTNode<T, keys, Cmp> &self, Dest &dest) {
  if (is_leaf(self)) {
    T *const subject = last(self.elements);
    assertx(subject);
    return take_leaf(self, subject, dest);
  }

  const size_t last_idx = length(self.elements);
  BTNode<T, keys, Cmp> *const greatest = self.children[last_idx];
  if (greatest) {
    const bool balance = take_max(*greatest, dest);
    if (balance) {
      T *const pivot = &self.elements[last_idx - 1];
      /* $greatest is right of $pivot */
      return rebalance(self, pivot, ChildDir_RIGHT);
    }

    return false;
  }

  assertx(false);
  return false;
}

static bool
take_min(BTNode<T, keys, Cmp> &self, Dest &dest) {
  if (is_leaf(self)) {
    T *const subject = &self.elements[0];
    assertx(subject);
    return take_leaf(self, subject, dest);
  }

  const size_t first_idx = 0;
  BTNode<T, keys, Cmp> *const smallest = self.children[first_idx];
  if (smallest) {
    const bool balance = take_min(*smallest, dest);
    if (balance) {
      T *const pivot = &self.elements[first_idx];
      /* $smallest is left of $pivot */
      return rebalance(self, pivot, ChildDir_LEFT);
    }

    return false;
  }

  assertx(false);
  return false;
}

static bool
take_int_node(BTNode<T, keys, Cmp> &self, T *subject, Dest &dest) {
  assertx(subject);
  auto &elements = self.elements;
  auto &children = self.children;

  const size_t index = index_of(elements, subject);
  assertxs(index != capacity(elements), index, length(elements));

  dest = std::move(*subject);

  BTNode<T, keys, Cmp> *const lt = children[index];
  if (lt) {
    assertxs(!is_empty(lt->elements), *subject);
    assertxs(!is_empty(lt->children), *subject);
    assertxs(!is_empty(*lt), *subject);

    const bool balance = take_max(*lt, *subject);
    if (balance) {
      return rebalance(self, subject, ChildDir_LEFT);
    }

    return false;
  }

  BTNode<T, keys, Cmp> *const gt = children[index + 1];
  if (gt) {
    assertxs(!is_empty(*gt), *subject);

    const bool balance = take_min(*gt, *subject);
    if (balance) {
      return rebalance(self, subject, ChildDir_RIGHT);
    }

    return false;
  }

  // should not get here
  assertx(false);
  return false;
}

static bool
take_wrapper(BTNode<T, keys, Cmp> &self, T *subject, Dest &dest) {
  if (is_leaf(self)) {
    return take_leaf(self, subject, dest);
  }

  return take_int_node(self, subject, dest);
}

static bool
take(BTNode<T, keys, C> *const tree, const Key &needle, Dest &dest,
     bool &balance) {
  if (tree == nullptr) {
    balance = false;
    return false;
  }

  auto &children = tree->children;
  auto &elements = tree->elements;

  C cmp;
  T *const gte = bin_find_gte<T, keys, Key, C>(elements, needle, cmp);
  if (gte) {
    if (!cmp(needle, *gte) && !cmp(*gte, needle)) {
      /* equal */

      balance = take_wrapper(*tree, gte, dest);
      return true;
    }

    /* Go down less than element child */
    const size_t index = index_of(elements, gte);
    assertxs(index != capacity(elements), index, length(elements));

    auto child = children[index];
    const bool result = take(child, needle, dest, balance);
    if (balance) {
      balance = rebalance(*tree, /*pivot*/ gte, ChildDir_LEFT);
    }
    return result;
  }

  /* needle is greater than any other element in elements */
  BTNode<T, keys, C> **child = last(children);
  assertxs(child, length(children));
  const bool result = take(*child, needle, dest, balance);
  if (balance) {
    T *const pivot = last(elements);
    assertx(pivot);
    balance = rebalance(*tree, pivot, ChildDir_RIGHT);
  }

  return result;
}

int
spfs_btree_remove(struct spfs_btree *tree, spfs_ino ino) {
  impl::BTreeNop<T> nop;
  bool balance = false;
  const bool result = impl::take(self.root, needle, nop, balance);
  if (balance) {
    BTNode<T, keys, Comparator> *const old = self.root;
    auto &elements = old->elements;

    if (is_empty(elements)) {
      auto &children = old->children;

      assertx(length(children) == 1);
      self.root = children[0];
      {
        clear(children);
        clear(old->elements);
        delete old;
      }
    }
  }

  return result;
}
