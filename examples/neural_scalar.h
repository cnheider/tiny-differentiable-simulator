#ifndef NEURAL_SCALAR_H
#define NEURAL_SCALAR_H

#include <iostream>
#include <map>

#include "tiny_neural_network.h"

/**
 * Implements a "neural network" scalar type that accepts input connections from
 * other NeuralScalars. The scalar can either be evaluated as residual module,
 * where the output of the neural network is combined with the value of the
 * scalar, or computed solely by the neural network ignoring the scalar's stored
 * value.
 */
template <typename Scalar, typename Utils>
class NeuralScalar {
 public:
  typedef ::TinyNeuralNetwork<Scalar, Utils> NeuralNetworkType;
  typedef Scalar InnerScalarType;
  typedef Utils InnerUtilsType;

 private:
  /**
   * Value assigned from outside.
   */
  Scalar value_{Utils::zero()};

  /**
   * Cached value from last evaluation.
   */
  mutable Scalar cache_;

  /**
   * Whether evaluation is necessary, or the cached value can be returned.
   */
  mutable bool is_dirty_{true};

  std::vector<NeuralScalar*> inputs_;

  NeuralNetworkType net_;

  /**
   * Neural scalars with the same name reuse the same neural network inputs,
   * parameters. No sharing takes place if the name is empty.
   */
  std::string name_;

  /**
   * This blueprint allows the user to specify neural network inputs and weights
   * to be used once a NeuralScalar with the given name is created.
   */
  struct NeuralBlueprint {
    std::vector<std::string> input_names;
    NeuralNetworkType net;
  };

  static inline std::map<std::string, NeuralScalar*> named_scalars_{};
  static inline std::map<std::string, NeuralBlueprint> blueprints_{};

  Scalar evaluate_network_() const {
    if (!name_.empty() && named_scalars_[name_] != this) {
      return named_scalars_[name_]->evaluate_network_();
    }
    static std::vector<Scalar> inputs(inputs_.size());
    for (std::size_t i = 0; i < inputs_.size(); ++i) {
      if (inputs_[i] == nullptr) continue;
      inputs[i] = inputs_[i]->evaluate();
    }
    static std::vector<Scalar> output(1);
    net_.compute(inputs, output);
    return output[0];
  }

 public:
  /**
   * Whether the internal value is added to, or replaced by, the neural
   * network's output.
   */
  bool is_residual{true};

  NeuralScalar() = default;

  inline NeuralScalar(const Scalar& value) : value_(value) { is_dirty_ = true; }

  NeuralScalar(const std::vector<NeuralScalar*>& inputs,
               bool use_input_bias = true)
      : inputs_(inputs),
        net_(static_cast<int>(inputs.size()), use_input_bias) {}
  NeuralScalar(const std::vector<NeuralScalar*>& inputs,
               const TinyNeuralNetworkSpecification& spec)
      : inputs_(inputs), net_(spec) {}

  // implement custom assignment operator to prevent internal data get wiped
  // by unwanted overwrite with a copy-constructed object
  NeuralScalar& operator=(const NeuralScalar& rhs) {
    value_ = rhs.evaluate();
    is_dirty_ = true;
    return *this;
  }
  NeuralScalar& operator=(const Scalar& rhs) {
    value_ = rhs;
    is_dirty_ = true;
    return *this;
  }
  NeuralScalar& operator=(double rhs) {
    value_ = Scalar(rhs);
    is_dirty_ = true;
    return *this;
  }

  const NeuralNetworkType& net() const { return net_; }
  NeuralNetworkType& net() { return net_; }

  /**
   * Add input connection to this neural network.
   */
  void connect(NeuralScalar* scalar,
               TinyNeuralNetworkActivation activation = NN_ACT_IDENTITY) {
    inputs_.push_back(scalar);
    net_.set_input_dim(net_.input_dim() + 1);
    // add output layer (if none has been created yet)
    if (net_.num_layers() == 1) {
      net_.add_linear_layer(activation, 1);
    }
    initialize();
    set_dirty();
  }

  void initialize(
      TinyNeuralNetworkInitialization init_method = NN_INIT_XAVIER) {
    net_.initialize(init_method);
  }

  bool is_dirty() const { return is_dirty_; }
  void set_dirty() {
    if (net_.output_dim() != 0) {
      is_dirty_ = true;
    }
  }

  /**
   * Retrieves neural network scalar by name, returns nullptr if no scalar with
   * such name exists.
   */
  static NeuralScalar* retrieve(const std::string& name) {
    if (named_scalars_.find(name) == named_scalars_.end()) {
      return nullptr;
    }
    return named_scalars_[name];
  }

  /**
   * Assigns a name to this scalar and looks up whether any blueprint
   * for this scalar has been defined to set up input connections and the neural
   * network.
   */
  void assign(const std::string& name) {
    name_ = name;
    if (blueprints_.find(name) != blueprints_.end()) {
      const NeuralBlueprint& blueprint = blueprints_[name];
      for (const std::string& input_name : blueprint.input_names) {
        NeuralScalar* input = retrieve(input_name);
        if (input == nullptr) {
          std::cerr << "NeuralScalar named \"" << input_name
                    << "\" has been requested before it was assigned.\n";
          assert(0);
        }
        inputs_.push_back(input);
      }
      net_ = blueprint.net;
    }
    named_scalars_[name] = this;
  }

  const Scalar& evaluate() const {
    if (!is_dirty_) {
      return cache_;
    }
    if (inputs_.empty()) {
      is_dirty_ = false;
      cache_ = value_;
      return value_;
    }
    Scalar net_output = evaluate_network_();
    cache_ = is_residual ? value_ + net_output : net_output;
    is_dirty_ = false;
    return cache_;
  }

  /**
   * Defines a neural network connection for a scalar with a given name.
   * Once this scalar is registered with this name using assign(), the
   * specified input connections are made and neural network with the given
   * weights and biases is set up for this scalar.
   */
  static void add_blueprint(const std::string& scalar_name,
                            const std::vector<std::string>& input_names,
                            const NeuralNetworkType& net) {
    NeuralBlueprint blueprint{input_names, net};
    blueprints_[scalar_name] = blueprint;
  }

  /// Scalar operators create plain NeuralScalars that do not have neural
  /// networks.

  inline friend NeuralScalar operator+(const NeuralScalar& lhs,
                                       const NeuralScalar& rhs) {
    return NeuralScalar(lhs.evaluate() + rhs.evaluate());
  }

  inline friend NeuralScalar operator-(const NeuralScalar& lhs,
                                       const NeuralScalar& rhs) {
    return NeuralScalar(lhs.evaluate() - rhs.evaluate());
  }

  inline friend NeuralScalar operator*(const NeuralScalar& lhs,
                                       const NeuralScalar& rhs) {
    return NeuralScalar(rhs.evaluate() * lhs.evaluate());
  }
  inline friend NeuralScalar operator/(const NeuralScalar& lhs,
                                       const NeuralScalar& rhs) {
    return NeuralScalar(lhs.evaluate() / rhs.evaluate());
  }

  inline friend bool operator<(const NeuralScalar& lhs,
                               const NeuralScalar& rhs) {
    return lhs.evaluate() < rhs.evaluate();
  }
  inline friend bool operator<=(const NeuralScalar& lhs,
                                const NeuralScalar& rhs) {
    return lhs.evaluate() <= rhs.evaluate();
  }
  inline friend bool operator>(const NeuralScalar& lhs,
                               const NeuralScalar& rhs) {
    return lhs.evaluate() > rhs.evaluate();
  }

  inline friend bool operator==(const NeuralScalar& lhs,
                                const NeuralScalar& rhs) {
    return lhs.evaluate() == rhs.evaluate();
  }
  inline friend bool operator!=(const NeuralScalar& lhs,
                                const NeuralScalar& rhs) {
    return lhs.evaluate() != rhs.evaluate();
  }

  inline friend NeuralScalar operator-(const NeuralScalar& rhs) {
    return NeuralScalar(-rhs.evaluate());
  }

  inline NeuralScalar& operator += (const NeuralScalar& lhs) {
    value_ += lhs.evaluate();
    is_dirty_ = true;
    return *this;
  }

  inline NeuralScalar& operator-=(const NeuralScalar& lhs) {
    value_ -= lhs.evaluate();
    is_dirty_ = true;
    return *this;
  }

  inline NeuralScalar& operator*=(const NeuralScalar& rhs) {
    value_ *= rhs.evaluate();
    is_dirty_ = true;
    return *this;
  }
  inline NeuralScalar& operator/=(const NeuralScalar& rhs) {
    value_ /= rhs.evaluate();
    is_dirty_ = true;
    return *this;
  }
};

template <typename Scalar, typename Utils>
struct NeuralScalarUtils {
  typedef ::NeuralScalar<Scalar, Utils> NeuralScalar;

  template <class T>
  static NeuralScalar fraction(T, T) = delete;  // C++11

  static NeuralScalar fraction(int num, int denom) {
    return scalar_from_double(double(num) / double(denom));
  }

  static NeuralScalar sin1(const NeuralScalar& v) {
    return Utils::sin1(v.evaluate());
  }
  static NeuralScalar cos1(const NeuralScalar& v) {
    return Utils::cos1(v.evaluate());
  }

  static NeuralScalar sqrt1(const NeuralScalar& v) {
    return Utils::sqrt1(v.evaluate());
  }

  static NeuralScalar zero() { return scalar_from_double(0.); }
  static NeuralScalar one() { return scalar_from_double(1.); }

  static NeuralScalar two() { return scalar_from_double(2.); }
  static NeuralScalar half() { return scalar_from_double(0.5); }
  static NeuralScalar pi() { return scalar_from_double(M_PI); }
  static NeuralScalar half_pi() { return scalar_from_double(M_PI / 2.); }

  static double getDouble(const NeuralScalar& v) {
    return Utils::getDouble(v.evaluate());
  }
  static inline NeuralScalar scalar_from_double(double value) {
    return NeuralScalar(Scalar(value));
  }

  template <class T>
  static NeuralScalar convert(T) = delete;  // C++11
  static NeuralScalar convert(int value) {
    return scalar_from_double((double)value);
  }

  static void FullAssert(bool a) {
    if (!a) {
      printf("!");
      assert(0);
      exit(0);
    }
  }

  static inline std::vector<NeuralScalar> to_neural(
      const std::vector<Scalar>& values) {
    std::vector<NeuralScalar> output(values.begin(), values.end());
    return output;
  }
  static inline std::vector<Scalar> from_neural(
      const std::vector<NeuralScalar>& values) {
    std::vector<Scalar> output(values.size());
    for (std::size_t i = 0; i < values.size(); ++i) {
      output[i] = values[i].evaluate();
    }
    return output;
  }
};

template <typename Scalar, typename Utils>
struct is_neural_scalar {
  static constexpr bool value = false;
};
template <typename Scalar, typename Utils>
struct is_neural_scalar<NeuralScalar<Scalar, Utils>,
                        NeuralScalarUtils<Scalar, Utils>> {
  static constexpr bool value = true;
};

#endif  // NEURAL_SCALAR_H