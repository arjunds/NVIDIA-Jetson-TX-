#ifndef CAFFE_PYTHON_LAYER_HPP_
#define CAFFE_PYTHON_LAYER_HPP_

#include <boost/python.hpp>
#include <vector>

#include "caffe/layer.hpp"

namespace bp = boost::python;

namespace caffe {

template <typename Dtype, typename Mtype>
class PythonLayer : public Layer<Dtype,Mtype> {
 public:
  PythonLayer(PyObject* self, const LayerParameter& param)
      : Layer<Dtype,Mtype>(param), self_(bp::handle<>(bp::borrowed(self))) { }

  virtual void LayerSetUp(const vector<Blob<Dtype,Mtype>*>& bottom,
      const vector<Blob<Dtype,Mtype>*>& top) {
    // Disallow PythonLayer in MultiGPU training stage, due to GIL issues
    // Details: https://github.com/BVLC/caffe/issues/2936
    if (this->phase_ == TRAIN && Caffe::solver_count() > 1
        && !ShareInParallel()) {
      LOG(FATAL) << "PythonLayer is not implemented in Multi-GPU training";
    }
    self_.attr("param_str") = bp::str(
        this->layer_param_.python_param().param_str());
    self_.attr("setup")(bottom, top);
  }
  virtual void Reshape(const vector<Blob<Dtype,Mtype>*>& bottom,
      const vector<Blob<Dtype,Mtype>*>& top) {
    self_.attr("reshape")(bottom, top);
  }

  virtual inline bool ShareInParallel() const {
    return this->layer_param_.python_param().share_in_parallel();
  }

  virtual inline const char* type() const { return "Python"; }

 protected:
  virtual void Forward_cpu(const vector<Blob<Dtype,Mtype>*>& bottom,
      const vector<Blob<Dtype,Mtype>*>& top) {
    self_.attr("forward")(bottom, top);
  }
  virtual void Backward_cpu(const vector<Blob<Dtype,Mtype>*>& top,
      const vector<bool>& propagate_down, const vector<Blob<Dtype,Mtype>*>& bottom) {
    self_.attr("backward")(top, propagate_down, bottom);
  }

 private:
  bp::object self_;
};

}  // namespace caffe

#endif
