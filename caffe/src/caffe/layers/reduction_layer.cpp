#include <algorithm>
#include <cfloat>
#include <vector>

#include "caffe/layer.hpp"
#include "caffe/util/math_functions.hpp"
#include "caffe/vision_layers.hpp"

namespace caffe {

template <typename Dtype, typename Mtype>
void ReductionLayer<Dtype,Mtype>::LayerSetUp(const vector<Blob<Dtype,Mtype>*>& bottom,
      const vector<Blob<Dtype,Mtype>*>& top) {
  op_ = this->layer_param_.reduction_param().operation();
}

template <typename Dtype, typename Mtype>
void ReductionLayer<Dtype,Mtype>::Reshape(const vector<Blob<Dtype,Mtype>*>& bottom,
      const vector<Blob<Dtype,Mtype>*>& top) {
  axis_ = bottom[0]->CanonicalAxisIndex(
      this->layer_param_.reduction_param().axis());
  // In the output, we'll keep all axes up to the reduction axis, but
  // throw away any after that.
  // Note: currently reducing along non-tail axes is not supported; otherwise,
  // we'd need to also copy any axes following an "end_axis".
  vector<int> top_shape(bottom[0]->shape().begin(),
                        bottom[0]->shape().begin() + axis_);
  top[0]->Reshape(top_shape);
  num_ = bottom[0]->count(0, axis_);
  dim_ = bottom[0]->count(axis_);
  CHECK_EQ(num_, top[0]->count());
  if (op_ == ReductionParameter_ReductionOp_SUM ||
      op_ == ReductionParameter_ReductionOp_MEAN) {
    vector<int> sum_mult_shape(1, dim_);
    sum_multiplier_.Reshape(sum_mult_shape);
    caffe_set(dim_, Get<Dtype>(1), sum_multiplier_.mutable_cpu_data());
  }
  coeff_ = Get<Dtype>(this->layer_param().reduction_param().coeff());
  if (op_ == ReductionParameter_ReductionOp_MEAN) {
    coeff_ /= Get<Dtype>(dim_);
  }
}

template <typename Dtype, typename Mtype>
void ReductionLayer<Dtype,Mtype>::Forward_cpu(
    const vector<Blob<Dtype,Mtype>*>& bottom, const vector<Blob<Dtype,Mtype>*>& top) {
  const Dtype* bottom_data = bottom[0]->cpu_data();
  const Dtype* mult_data = NULL;
  if (sum_multiplier_.count() > 0) {
    mult_data = sum_multiplier_.cpu_data();
  }
  Dtype* top_data = top[0]->mutable_cpu_data();
  for (int i = 0; i < num_; ++i) {
    switch (op_) {
    case ReductionParameter_ReductionOp_SUM:
    case ReductionParameter_ReductionOp_MEAN:
      *top_data = Get<Dtype>(caffe_cpu_dot<Dtype,Mtype>(dim_, mult_data, bottom_data));
      break;
    case ReductionParameter_ReductionOp_ASUM:
      *top_data = Get<Dtype>(caffe_cpu_asum<Dtype,Mtype>(dim_, bottom_data));
      break;
    case ReductionParameter_ReductionOp_SUMSQ:
      *top_data = Get<Dtype>(caffe_cpu_dot<Dtype,Mtype>(dim_, bottom_data, bottom_data));
      break;
    default:
      LOG(FATAL) << "Unknown reduction op: "
          << ReductionParameter_ReductionOp_Name(op_);
    }
    bottom_data += dim_;
    ++top_data;
  }
  if (coeff_ != Get<Dtype>(1)) {
    // Reset the top_data pointer.
    top_data = top[0]->mutable_cpu_data();
    caffe_scal<Dtype,Mtype>(num_, Get<Mtype>(coeff_), top_data);
  }
}

template <typename Dtype, typename Mtype>
void ReductionLayer<Dtype,Mtype>::Backward_cpu(const vector<Blob<Dtype,Mtype>*>& top,
    const vector<bool>& propagate_down, const vector<Blob<Dtype,Mtype>*>& bottom) {
  if (!propagate_down[0]) { return; }
  // Get bottom_data, if needed.
  const Dtype* bottom_data = NULL;
  switch (op_) {
  // Operations that don't need bottom_data
  case ReductionParameter_ReductionOp_SUM:
  case ReductionParameter_ReductionOp_MEAN:
    break;
  // Operations that need bottom_data
  case ReductionParameter_ReductionOp_ASUM:
  case ReductionParameter_ReductionOp_SUMSQ:
    bottom_data = bottom[0]->cpu_data();
    break;
  default:
    LOG(FATAL) << "Unknown reduction op: "
        << ReductionParameter_ReductionOp_Name(op_);
  }
  const Dtype* top_diff = top[0]->cpu_diff();
  Dtype* bottom_diff = bottom[0]->mutable_cpu_diff();
  for (int i = 0; i < num_; ++i) {
    const Mtype bottom_coeff = Get<Mtype>((*top_diff) * coeff_);
    switch (op_) {
    case ReductionParameter_ReductionOp_SUM:
    case ReductionParameter_ReductionOp_MEAN:
      caffe_set(dim_, Get<Dtype>(bottom_coeff), bottom_diff);
      break;
    case ReductionParameter_ReductionOp_ASUM:
      caffe_cpu_sign<Dtype,Mtype>(dim_, bottom_data, bottom_diff);
      caffe_scal<Dtype,Mtype>(dim_, bottom_coeff, bottom_diff);
      break;
    case ReductionParameter_ReductionOp_SUMSQ:
      caffe_cpu_scale<Dtype,Mtype>(dim_, Mtype(2 * bottom_coeff), bottom_data, bottom_diff);
      break;
    default:
      LOG(FATAL) << "Unknown reduction op: "
          << ReductionParameter_ReductionOp_Name(op_);
    }
    bottom_data += dim_;
    bottom_diff += dim_;
    ++top_diff;
  }
}

#ifdef CPU_ONLY
STUB_GPU(ReductionLayer);
#endif

INSTANTIATE_CLASS(ReductionLayer);
REGISTER_LAYER_CLASS(Reduction);

}  // namespace caffe
