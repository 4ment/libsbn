// Copyright 2019-2022 bito project contributors.
// bito is free software under the GPLv3; see LICENSE file for details.

#include "nni_operation.hpp"

#include "bitset.hpp"
#include "subsplit_dag.hpp"

// ** NNIOperation

int NNIOperation::Compare(const NNIOperation &nni_a, const NNIOperation &nni_b) {
  auto compare_parent = Bitset::SubsplitCompare(nni_a.parent_, nni_b.parent_);
  if (compare_parent != 0) {
    return compare_parent;
  }
  auto compare_child = Bitset::SubsplitCompare(nni_a.child_, nni_b.child_);
  return compare_child;
};

int NNIOperation::Compare(const NNIOperation &nni_b) const {
  const NNIOperation &nni_a = *this;
  return Compare(nni_a, nni_b);
}

bool operator<(const NNIOperation &lhs, const NNIOperation &rhs) {
  return NNIOperation::Compare(lhs, rhs) < 0;
}
bool operator<=(const NNIOperation &lhs, const NNIOperation &rhs) {
  return NNIOperation::Compare(lhs, rhs) <= 0;
}
bool operator>(const NNIOperation &lhs, const NNIOperation &rhs) {
  return NNIOperation::Compare(lhs, rhs) > 0;
}
bool operator>=(const NNIOperation &lhs, const NNIOperation &rhs) {
  return NNIOperation::Compare(lhs, rhs) >= 0;
}
bool operator==(const NNIOperation &lhs, const NNIOperation &rhs) {
  return NNIOperation::Compare(lhs, rhs) == 0;
}
bool operator!=(const NNIOperation &lhs, const NNIOperation &rhs) {
  return NNIOperation::Compare(lhs, rhs) != 0;
}

NNIOperation NNIOperation::NNIOperationFromNeighboringSubsplits(
    const Bitset parent_in, const Bitset child_in,
    const bool is_sister_swapped_with_right_child, const bool is_focal_clade_on_right) {
  // Input: Parent(X,YZ) -> Child(Y,Z).
  Bitset X = parent_in.SubsplitGetClade(!is_focal_clade_on_right);
  // "Y" clade can be chosen arbitrarily from (Y,Z), so "Y" is chosen based on which
  // we want to swap with "X".
  Bitset Y = child_in.SubsplitGetClade(is_sister_swapped_with_right_child);
  Bitset Z = child_in.SubsplitGetClade(!is_sister_swapped_with_right_child);
  // Output: Parent(Y,XZ) -> Child(X,Z).
  Bitset parent_out = Bitset::Subsplit(Y, X | Z);
  Bitset child_out = Bitset::Subsplit(X, Z);
  return NNIOperation(parent_out, child_out);
}

NNIOperation NNIOperation::NNIOperationFromNeighboringSubsplits(
    const Bitset parent_in, const Bitset child_in,
    const bool is_sister_swapped_with_right_child) {
  SubsplitClade focal_clade_of_parent =
      Bitset::SubsplitIsChildOfWhichParentClade(parent_in, child_in);
  const bool is_focal_clade_on_right = (focal_clade_of_parent == SubsplitClade::Right);
  return NNIOperationFromNeighboringSubsplits(
      parent_in, child_in, is_sister_swapped_with_right_child, is_focal_clade_on_right);
}

NNIOperation NNIOperation::NNIOperationFromNeighboringSubsplits(
    const bool is_sister_swapped_with_right_child) const {
  return NNIOperation::NNIOperationFromNeighboringSubsplits(
      parent_, child_, is_sister_swapped_with_right_child);
}

// ** Query

bool NNIOperation::AreNNIOperationsNeighbors(const NNIOperation &nni_a,
                                             const NNIOperation &nni_b) {
  if (nni_a.GetSisterClade() == nni_b.GetSisterClade()) {
    return false;
  }
  std::array<Bitset, 3> vec_a = {nni_a.GetSisterClade(), nni_a.GetLeftChildClade(),
                                 nni_a.GetRightChildClade()};
  std::array<Bitset, 3> vec_b = {nni_b.GetSisterClade(), nni_b.GetLeftChildClade(),
                                 nni_b.GetRightChildClade()};
  std::sort(vec_a.begin(), vec_a.end());
  std::sort(vec_b.begin(), vec_b.end());
  return (vec_a == vec_b);
};

bool NNIOperation::WhichCladeSwapWithSisterToCreatePostNNI(
    const NNIOperation &pre_nni, const NNIOperation &post_nni) {
  Assert(AreNNIOperationsNeighbors(pre_nni, post_nni),
         "Given NNIs must be neighbors to find clade swap.");
  const Bitset &pre_sister = pre_nni.GetSisterClade();
  bool is_sister_swapped_with_right_child =
      (pre_sister == post_nni.GetRightChildClade());
  return is_sister_swapped_with_right_child;
};

// ** Miscellaneous

NNIOperation::NNICladeArray NNIOperation::BuildNNICladeMapFromPreNNIToNNI(
    const NNIOperation &pre_nni, const NNIOperation &post_nni) {
  Assert(AreNNIOperationsNeighbors(pre_nni, post_nni),
         "Given NNIs must be neighbors to find clade map.");
  NNICladeArray nni_clade_map;
  EnumArray<NNIClade, NNICladeCount, bool> mapped_post_clades;
  mapped_post_clades.fill(false);
  const std::array<NNIClade, 3> mappable_clades = {
      NNIClade::ParentSister, NNIClade::ChildLeft, NNIClade::ChildRight};
  for (const NNIClade pre_nni_clade_type : mappable_clades) {
    const Bitset &pre_nni_clade = pre_nni.GetClade(pre_nni_clade_type);
    bool is_found = false;
    for (const NNIClade post_nni_clade_type : mappable_clades) {
      if (mapped_post_clades[post_nni_clade_type]) {
        continue;
      }
      const Bitset &post_nni_clade = post_nni.GetClade(post_nni_clade_type);
      if (pre_nni_clade == post_nni_clade) {
        is_found = true;
        nni_clade_map[pre_nni_clade_type] = post_nni_clade_type;
        mapped_post_clades[post_nni_clade_type] = true;
        break;
      }
    }
    Assert(is_found,
           "Unexpected Error: Was not able to find a clade mapping from pre_nni to "
           "post_nni.");
  }
  nni_clade_map[NNIClade::ParentFocal] = NNIClade::ParentFocal;
  return nni_clade_map;
};

bool NNIOperation::IsValid() {
  return Bitset::SubsplitIsParentChildPair(parent_, child_);
};

std::string NNIOperation::ToString() const {
  std::stringstream os;
  os << "{ P:" << parent_.SubsplitToString() << ", C:" << child_.SubsplitToString()
     << " }";
  return os.str();
}

std::ostream &operator<<(std::ostream &os, const NNIOperation &nni) {
  return os << nni.ToString();
};
