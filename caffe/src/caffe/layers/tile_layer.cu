#include <vector>

#include "caffe/common_layers.hpp"
#include "caffe/layer.hpp"
#include "caffe/util/math_functions.hpp"

namespace caffe {

template <typename Dtype, typename Mtype>
__global__ void Tile(const int nthreads, const Dtype* bottom_data,
    const int tile_size, const int num_tiles, const int bottom_tile_axis,
    Dtype* top_data) {
  CUDA_KERNEL_LOOP(index, nthreads) {
    const int d = index % tile_size;
    const int b = (index / tile_size / num_tiles) % bottom_tile_axis;
    const int n = index / tile_size / num_tiles / bottom_tile_axis;
    const int bottom_index = (n * bottom_tile_axis + b) * tile_size + d;
    top_data[index] = bottom_data[bottom_index];
  }
}

template <typename Dtype, typename Mtype>
void TileLayer<Dtype,Mtype>::Forward_gpu(
    const vector<Blob<Dtype,Mtype>*>& bottom, const vector<Blob<Dtype,Mtype>*>& top) {
  const Dtype* bottom_data = bottom[0]->gpu_data();
  Dtype* top_data = top[0]->mutable_gpu_data();
  const int bottom_tile_axis = bottom[0]->shape(axis_);
  const int nthreads = top[0]->count();
  Tile<Dtype,Mtype>  // NOLINT_NEXT_LINE(whitespace/operators)
      <<<CAFFE_GET_BLOCKS(nthreads), CAFFE_CUDA_NUM_THREADS>>>(
      nthreads, bottom_data, inner_dim_, tiles_, bottom_tile_axis, top_data);
}

template <typename Dtype, typename Mtype>
__global__ void TileBackward(const int nthreads, const Dtype* top_diff,
    const int tile_size, const int num_tiles, const int bottom_tile_axis,
    Dtype* bottom_diff) {
  CUDA_KERNEL_LOOP(index, nthreads) {
    const int d = index % tile_size;
    const int b = (index / tile_size) % bottom_tile_axis;
    const int n = index / tile_size / bottom_tile_axis;
    bottom_diff[index] = Get<Dtype>(0);
    int top_index = (n * num_tiles * bottom_tile_axis + b) * tile_size + d;
    for (int t = 0; t < num_tiles; ++t) {
      bottom_diff[index] += top_diff[top_index];
      top_index += bottom_tile_axis * tile_size;
    }
  }
}

template <typename Dtype, typename Mtype>
void TileLayer<Dtype,Mtype>::Backward_gpu(const vector<Blob<Dtype,Mtype>*>& top,
    const vector<bool>& propagate_down, const vector<Blob<Dtype,Mtype>*>& bottom) {
  if (!propagate_down[0]) { return; }
  const Dtype* top_diff = top[0]->gpu_diff();
  Dtype* bottom_diff = bottom[0]->mutable_gpu_diff();
  const int bottom_tile_axis = bottom[0]->shape(axis_);
  const int tile_size = inner_dim_ / bottom_tile_axis;
  const int nthreads = bottom[0]->count();
  TileBackward<Dtype,Mtype>  // NOLINT_NEXT_LINE(whitespace/operators)
      <<<CAFFE_GET_BLOCKS(nthreads), CAFFE_CUDA_NUM_THREADS>>>(
      nthreads, top_diff, tile_size, tiles_, bottom_tile_axis, bottom_diff);
}

INSTANTIATE_LAYER_GPU_FUNCS(TileLayer);

}  // namespace caffe
