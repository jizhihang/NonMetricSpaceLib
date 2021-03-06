/**
 * Non-metric Space Library
 *
 * Authors: Bilegsaikhan Naidan (https://github.com/bileg), Leonid Boytsov (http://boytsov.info).
 * With contributions from Lawrence Cayton (http://lcayton.com/) and others.
 *
 * For the complete list of contributors and further details see:
 * https://github.com/searchivarius/NonMetricSpaceLib 
 * 
 * Copyright (c) 2014
 *
 * This code is released under the
 * Apache License Version 2.0 http://www.apache.org/licenses/.
 *
 */
#include <algorithm>
#include <iostream>
#include <sstream>
#include <vector>
#include <unordered_set>
#include <memory>

#include "space.h"
#include "knnqueue.h"
#include "knnquery.h"
#include "rangequery.h"
#include "methodfactory.h"
#include "method/multi_index.h"

namespace similarity {

using std::unique_ptr;

template <typename dist_t>
MultiIndex<dist_t>::MultiIndex(
         const string& SpaceType,
         const Space<dist_t>* space, 
         const ObjectVector& data,
         const AnyParams& AllParams) : space_(space) {
  bool            PrintProgress = false;
  AnyParamManager pmgr(AllParams);

  pmgr.GetParamRequired("indexQty", IndexQty_);
  pmgr.GetParamRequired("methodName", MethodName_);
  pmgr.GetParamOptional("printProgress", PrintProgress);

  AnyParams RemainParams = pmgr.ExtractParametersExcept( {"indexQty", "methodName", "printProgress"} );


  for (size_t i = 0; i < IndexQty_; ++i) {
    LOG(LIB_INFO) << "Method: " << MethodName_ << " index # " << (i+1) << " out of " << IndexQty_;
    indices_.push_back(MethodFactoryRegistry<dist_t>::Instance().CreateMethod(PrintProgress, 
                                                                 MethodName_,
                                                                 SpaceType,
                                                                 space,
                                                                 data,
                                                                 RemainParams));
  }

}

template <typename dist_t>
MultiIndex<dist_t>::~MultiIndex() {
  for (size_t i = 0; i < indices_.size(); ++i) 
    delete indices_[i];
}

template <typename dist_t>
const std::string MultiIndex<dist_t>::ToString() const {
  std::stringstream str;
  str << "" << indices_.size() << " copies of " << MethodName_;
  return str.str();
}

template <typename dist_t>
void MultiIndex<dist_t>::Search(RangeQuery<dist_t>* query) {
  /* 
   * There may be duplicates: the same object coming from 
   * different indices. The set found is used to filter them out.
   */
  std::unordered_set<const Object*> found;

  for (size_t i = 0; i < indices_.size(); ++i) {
    RangeQuery<dist_t>  TmpRes(space_, query->QueryObject(), query->Radius());
    indices_[i]->Search(&TmpRes);
    const ObjectVector& res          = *TmpRes.Result();
    const std::vector<dist_t>& dists = *TmpRes.ResultDists();

    query->AddDistanceComputations(TmpRes.DistanceComputations());
    for (size_t i = 0; i < res.size(); ++i) {
      const Object* obj = res[i];
      if (!found.count(obj)) {
        query->CheckAndAddToResult(dists[i], obj);
        found.insert(obj);
      }
    }
  }
}

template <typename dist_t>
void MultiIndex<dist_t>::Search(KNNQuery<dist_t>* query) {
  /* 
   * There may be duplicates: the same object coming from 
   * different indices. The set found is used to filter them out.
   */
  std::unordered_set<IdType> found;

  for (size_t i = 0; i < indices_.size(); ++i) {
    KNNQuery<dist_t> TmpRes(space_, query->QueryObject(), query->GetK(), query->GetEPS());

    indices_[i]->Search(&TmpRes);
    unique_ptr<KNNQueue<dist_t>> ResQ(TmpRes.Result()->Clone());

    query->AddDistanceComputations(TmpRes.DistanceComputations());
    while(!ResQ->Empty()) {
      const Object* obj = reinterpret_cast<const Object*>(ResQ->TopObject());

      if (!found.count(obj->id())) {
        query->CheckAndAddToResult(ResQ->TopDistance(), obj);
        found.insert(obj->id());
      }
      ResQ->Pop();
    }
  }
}

template class MultiIndex<float>;
template class MultiIndex<double>;
template class MultiIndex<int>;

}   // namespace similarity
