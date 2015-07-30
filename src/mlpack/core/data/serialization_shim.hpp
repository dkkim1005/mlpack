/**
 * @file serialization_shim.hpp
 * @author Ryan Curtin
 *
 * This file contains the necessary shims to make boost.serialization work with
 * classes that have a Serialize() method (instead of a serialize() method).
 *
 * This allows our mlpack naming conventions to remain intact, and only costs a
 * small amount of ridiculous template metaprogramming.
 */
#ifndef __MLPACK_CORE_UTIL_SERIALIZATION_SHIM_HPP
#define __MLPACK_CORE_UTIL_SERIALIZATION_SHIM_HPP

#include <mlpack/core/util/sfinae_utility.hpp>
#include <boost/serialization/serialization.hpp>
#include <boost/archive/xml_oarchive.hpp>

namespace mlpack {
namespace data {

// This gives us a HasSerialize<T, U> type (where U is a function pointer) we
// can use with SFINAE to catch when a type has a Serialize() function.
HAS_MEM_FUNC(Serialize, HasSerialize);

// Declare the shims we need.
template<typename T> class FirstShim;
template<typename T> class FirstArrayShim;
template<typename T> class FirstNormalArrayShim;
template<typename T> class SecondShim;
template<typename T> class SecondArrayShim;
template<typename T> class SecondNormalArrayShim;
template<typename T> class PointerShim;

/**
 * Call this function to produce a name-value pair; this is similar to
 * BOOST_SERIALIZATION_NVP(), but should be used for types that have a
 * Serialize() function (or contain a type that has a Serialize() function)
 * instead of a serialize() function.  The template type should be automatically
 * deduced, and the two boost::enable_if<> parameters are automatically deduced
 * too.  So usage looks like
 *
 * @code
 * MyType t;
 * CreateNVP(t, "my_name_for_t");
 * @endcode
 *
 * Note that the second parameter, 'name', must be a valid XML identifier.
 *
 * This function does not return a boost::serialization::nvp<T> object, but
 * instead a shim type (FirstShim<T>).
 *
 * This particular overload is used by classes that have a Serialize() function.
 *
 * @param t Object to create NVP (name-value pair) with.
 * @param name Name of object (must be a valid XML identifier).
 */
template<typename T>
inline FirstShim<T> CreateNVP(
    T& t,
    const std::string& name,
    typename boost::enable_if<boost::is_class<T>>::type* = 0,
    typename boost::enable_if<HasSerialize<T,
        void(T::*)(boost::archive::xml_oarchive&, const unsigned int)>>::
        type* = 0)
{
  return FirstShim<T>(t, name);
}

/**
 * Call this function to produce a name-value pair; this is similar to
 * BOOST_SERIALIZATION_NVP(), but should be used for types that have a
 * Serialize() function (or contain a type that has a Serialize() function)
 * instead of a serialize() function.  The template type should be automatically
 * deduced, and the two boost::enable_if<> parameters are automatically deduced
 * too.  So usage looks like
 *
 * @code
 * MyType t;
 * CreateNVP(t, "my_name_for_t");
 * @endcode
 *
 * Note that the second parameter, 'name', must be a valid XML identifier.
 *
 * This particular overload is used by classes that do not have a Serialize()
 * function (so, no shim is necessary).
 *
 * @param t Object to create NVP (name-value pair) with.
 * @param name Name of object (must be a valid XML identifier).
 */
template<typename T>
inline
#ifndef BOOST_NO_FUNCTION_TEMPLATE_ORDERING
const // Imitate the boost::serialization make_nvp() function.
#endif
boost::serialization::nvp<T> CreateNVP(
    T& t,
    const std::string& name,
    typename boost::enable_if<boost::is_class<T>>::type* = 0,
    typename boost::disable_if<HasSerialize<T,
        void(T::*)(boost::archive::xml_oarchive&, const unsigned int)>>::
        type* = 0)
{
  return boost::serialization::make_nvp(name.c_str(), t);
}

/**
 * Call this function to produce a name-value pair; this is similar to
 * BOOST_SERIALIZATION_NVP(), but should be used for types that have a
 * Serialize() function (or contain a type that has a Serialize() function)
 * instead of a serialize() function.  The template type should be automatically
 * deduced, and the two boost::enable_if<> parameters are automatically deduced
 * too.  So usage looks like
 *
 * @code
 * MyType t;
 * CreateNVP(t, "my_name_for_t");
 * @endcode
 *
 * Note that the second parameter, 'name', must be a valid XML identifier.
 *
 * This particular overload is used by primitive types that aren't pointers.
 *
 * @param t Object to create NVP (name-value pair) with.
 * @param name Name of object (must be a valid XML identifier).
 */
template<typename T>
inline
#ifndef BOOST_NO_FUNCTION_TEMPLATE_ORDERING
const // Imitate the boost::serialization make_nvp() function.
#endif
boost::serialization::nvp<T> CreateNVP(
    T& t,
    const std::string& name,
    typename boost::disable_if<boost::is_class<T>>::type* = 0,
    typename boost::disable_if<boost::is_pointer<T>>::type* = 0)
{
  return boost::serialization::make_nvp(name.c_str(), t);
}

/**
 * Call this function to produce a name-value pair; this is similar to
 * BOOST_SERIALIZATION_NVP(), but should be used for types that have a
 * Serialize() function (or contain a type that has a Serialize() function)
 * instead of a serialize() function.  The template type should be automatically
 * deduced, and the two boost::enable_if<> parameters are automatically deduced
 * too.  So usage looks like
 *
 * @code
 * MyType t;
 * CreateNVP(t, "my_name_for_t");
 * @endcode
 *
 * Note that the second parameter, 'name', must be a valid XML identifier.
 *
 * This particular overload is used by pointers to classes that have a
 * Serialize() function.
 *
 * @param t Object to create NVP (name-value pair) with.
 * @param name Name of object (must be a valid XML identifier).
 */
template<typename T>
inline
#ifndef BOOST_NO_FUNCTION_TEMPLATE_ORDERING
const
#endif
boost::serialization::nvp<PointerShim<T>*> CreateNVP(
    T*& t,
    const std::string& name,
    typename boost::enable_if<boost::is_class<T>>::type* = 0,
    typename boost::enable_if<HasSerialize<T,
        void(T::*)(boost::archive::xml_oarchive&, const unsigned int)>>::
        type* = 0)
{
  return boost::serialization::make_nvp(name.c_str(),
      reinterpret_cast<PointerShim<T>*&>(t));
}

/**
 * Call this function to produce a name-value pair; this is similar to
 * BOOST_SERIALIZATION_NVP(), but should be used for types that have a
 * Serialize() function (or contain a type that has a Serialize() function)
 * instead of a serialize() function.  The template type should be automatically
 * deduced, and the two boost::enable_if<> parameters are automatically deduced
 * too.  So usage looks like
 *
 * @code
 * MyType t;
 * CreateNVP(t, "my_name_for_t");
 * @endcode
 *
 * Note that the second parameter, 'name', must be a valid XML identifier.
 *
 * This particular overload is used by pointers to classes that do not have a
 * Serialize() function.
 *
 * @param t Object to create NVP (name-value pair) with.
 * @param name Name of object (must be a valid XML identifier).
 */
template<typename T>
inline
#ifndef BOOST_NO_FUNCTION_TEMPLATE_ORDERING
const
#endif
boost::serialization::nvp<T*> CreateNVP(
    T*& t,
    const std::string& name,
    typename boost::enable_if<boost::is_class<T>>::type* = 0,
    typename boost::disable_if<HasSerialize<T,
        void(T::*)(boost::archive::xml_oarchive&, const unsigned int)>>::
        type* = 0)
{
  return boost::serialization::make_nvp(name.c_str(), t);
}

/**
 * Call this function to produce a name-value pair; this is similar to
 * BOOST_SERIALIZATION_NVP(), but should be used for types that have a
 * Serialize() function (or contain a type that has a Serialize() function)
 * instead of a serialize() function.  The template type should be automatically
 * deduced, and the two boost::enable_if<> parameters are automatically deduced
 * too.  So usage looks like
 *
 * @code
 * MyType t;
 * CreateNVP(t, "my_name_for_t");
 * @endcode
 *
 * Note that the second parameter, 'name', must be a valid XML identifier.
 *
 * This particular overload is used by pointers to non-classes.
 *
 * @param t Object to create NVP (name-value pair) with.
 * @param name Name of object (must be a valid XML identifier).
 */
template<typename T>
inline
#ifndef BOOST_NO_FUNCTION_TEMPLATE_ORDERING
const
#endif
boost::serialization::nvp<T*> CreateNVP(
    T*& t,
    const std::string& name,
    typename boost::disable_if<boost::is_class<T>>::type* = 0)
{
  return boost::serialization::make_nvp(name.c_str(), t);
}

/**
 * Call this function to produce a name-value pair for an array; this is similar
 * to boost::serialization::make_array(), but provides a nicer wrapper, allows
 * types that have a Serialize() function, and allows you to give a name to your
 * array.  This particular overload is used by classes that have a Serialize()
 * function.
 */
template<typename T>
inline FirstArrayShim<T> CreateArrayNVP(
    T* t,
    const size_t len,
    const std::string& name,
    typename boost::enable_if<boost::is_class<T>>::type* = 0,
    typename boost::enable_if<HasSerialize<T,
        void(T::*)(boost::archive::xml_oarchive&, const unsigned int)>>::
        type* = 0)
{
  return FirstArrayShim<T>(t, len, name);
}

/**
 * Call this function to produce a name-value pair for an array; this is similar
 * to boost::serialization::make_array(), but provides a nicer wrapper, allows
 * types that have a Serialize() function, and allows you to give a name to your
 * array.  This particular overload is used by classes that do not have a
 * Serialize() function.
 */
template<typename T>
inline FirstNormalArrayShim<T> CreateArrayNVP(
    T* t,
    const size_t len,
    const std::string& name,
    typename boost::enable_if<boost::is_class<T>>::type* = 0,
    typename boost::disable_if<HasSerialize<T,
        void(T::*)(boost::archive::xml_oarchive&, const unsigned int)>>::
        type* = 0)
{
  return FirstNormalArrayShim<T>(t, len, name);
}

/**
 * Call this function to produce a name-value pair for an array; this is similar
 * to boost::serialization::make_array(), but provides a nicer wrapper, allows
 * types that have a Serialize() function, and allows you to give a name to your
 * array.  This particular overload is used by primitive types.
 */
template<typename T>
inline FirstNormalArrayShim<T> CreateArrayNVP(
    T* t,
    const size_t len,
    const std::string& name,
    typename boost::disable_if<boost::is_class<T>>::type* = 0)
{
  return FirstNormalArrayShim<T>(t, len, name);
}

/**
 * The first shim: simply holds the object and its name.  This shim's purpose is
 * to be caught by our overloads of operator<<, operator&, and operator>>, which
 * then creates a second shim.
 */
template<typename T>
struct FirstShim
{
  //! Construct the first shim with the given object and name.
  FirstShim(T& t, const std::string& name) : t(t), name(name) { }

  T& t;
  const std::string& name;
};

/**
 * A first shim for arrays.  This shim's purpose is to be caught by our
 * overloads of operator<<, operator&, and operator>>, which then creates a
 * second shim.
 */
template<typename T>
struct FirstArrayShim
{
  //! Construct the first shim with the given objects, length, and name.
  FirstArrayShim(T* t, const size_t len, const std::string& name) :
      t(t), len(len), name(name) { }

  T* t;
  const size_t len;
  const std::string& name;
};

/**
 * A first shim for arrays without a Serialize() method.  This shim's purpose is
 * to be caught by our overloads of operator<<, operator&, and operator>>, which
 * then creates a second shim.
 */
template<typename T>
struct FirstNormalArrayShim
{
  //! Construct the first shim with the given objects, length, and name.
  FirstNormalArrayShim(T* t, const size_t len, const std::string& name) :
      t(t), len(len), name(name) { }

  T* t;
  const size_t len;
  const std::string& name;
};

/**
 * The second shim: wrap the call to Serialize() inside of a serialize()
 * function, so that an archive type can call serialize() on a SecondShim object
 * and this gets forwarded correctly to our object's Serialize() function.
 */
template<typename T>
struct SecondShim
{
  //! Construct the second shim.  The name isn't necessary for this shim.
  SecondShim(T& t) : t(t) { }

  //! A wrapper for t.Serialize().
  template<typename Archive>
  void serialize(Archive& ar, const unsigned int version)
  {
    t.Serialize(ar, version);
  }

  T& t;
};

/**
 * A shim for objects in an array; this is basically like the SecondShim, but
 * for arrays that hold objects that have Serialize() methods instead of
 * serialize() methods.
 */
template<typename T>
struct SecondArrayShim
{
  //! Construct the shim.
  SecondArrayShim(T* t, const size_t len) : t(t), len(len) { }

  //! A wrapper for Serialize() for each element.
  template<typename Archive>
  void serialize(Archive& ar, const unsigned int /* version */)
  {
    // Serialize each element, using the shims we already have.
    for (size_t i = 0; i < len; ++i)
      ar & CreateNVP(t[i], "item");
  }

  T* t;
  const size_t len;
};

/**
 * A shim for objects in an array which do not have a Serialize() function.
 * This is like the SecondShim class.
 */
template<typename T>
struct SecondNormalArrayShim
{
  //! Construct the shim.
  SecondNormalArrayShim(T* t, const size_t len) : t(t), len(len) { }

  //! A wrapper for make_array().
  template<typename Archive>
  void serialize(Archive& ar, const unsigned int /* version */)
  {
    ar & boost::serialization::make_array(t, len);
  }

  T* t;
  const size_t len;
};

/**
 * A shim for pointers.  Only the type of this shim is used, so it is created
 * with a static_cast<>, and then later static_cast<>ed back to the original
 * type.
 */
template<typename T>
struct PointerShim : public T { };

/**
 * Catch when we call operator<< with a FirstShim object.  In this case, we make
 * the second-level shim and use it.  Note that this second-level shim can be
 * used as an lvalue, which is what's necessary for this whole thing to work.
 * The first-level shim can't be an lvalue (this is why we need two levels of
 * shims).
 */
template<typename Archive, typename T>
Archive& operator<<(Archive& ar, FirstShim<T> t)
{
  SecondShim<T> sh(t.t);
  return (ar << boost::serialization::make_nvp(t.name.c_str(), sh));
}

/**
 * Catch when we call operator& with a FirstShim object.  In this case, we make
 * the second-level shim and use it.  Note that this second-level shim can be
 * used as an lvalue, which is what's necessary for this whole thing to work.
 * The first-level shim can't be an lvalue (this is why we need two levels of
 * shims).
 */
template<typename Archive, typename T>
Archive& operator&(Archive& ar, FirstShim<T> t)
{
  SecondShim<T> sh(t.t);
  return (ar & boost::serialization::make_nvp(t.name.c_str(), sh));
}

/**
 * Catch when we call operator>> with a FirstShim object.  In this case, we make
 * the second-level shim and use it.  Note that this second-level shim can be
 * used as an lvalue, which is what's necessary for this whole thing to work.
 * The first-level shim can't be an lvalue (this is why we need two levels of
 * shims).
 */
template<typename Archive, typename T>
Archive& operator>>(Archive& ar, FirstShim<T> t)
{
  SecondShim<T> sh(t.t);
  return (ar >> boost::serialization::make_nvp(t.name.c_str(), sh));
}

/**
 * Catch when we call operator<< with a FirstArrayShim object.  In this case, we
 * make the second-level array shim and use it.  Note that this second-level
 * shim can be used as an lvalue, which is what's necessary for this whole thing
 * to work.  The first-level shim can't be an lvalue (this is why we need two
 * levels of shims).
 */
template<typename Archive, typename T>
Archive& operator<<(Archive& ar, FirstArrayShim<T> t)
{
  SecondArrayShim<T> sh(t.t, t.len);
  return (ar << boost::serialization::make_nvp(t.name.c_str(), sh));
}

/**
 * Catch when we call operator& with a FirstArrayShim object.  In this case, we
 * make the second-level array shim and use it.  Note that this second-level
 * shim can be used as an lvalue, which is what's necessary for this whole thing
 * to work.  The first-level shim can't be an lvalue (this is why we need two
 * levels of shims).
 */
template<typename Archive, typename T>
Archive& operator&(Archive& ar, FirstArrayShim<T> t)
{
  SecondArrayShim<T> sh(t.t, t.len);
  return (ar & boost::serialization::make_nvp(t.name.c_str(), sh));
}

/**
 * Catch when we call operator>> with a FirstArrayShim object.  In this case, we
 * make the second-level array shim and use it.  Note that this second-level
 * shim can be used as an lvalue, which is what's necessary for this whole thing
 * to work.  The first-level shim can't be an lvalue (this is why we need two
 * levels of shims).
 */
template<typename Archive, typename T>
Archive& operator>>(Archive& ar, FirstArrayShim<T> t)
{
  SecondArrayShim<T> sh(t.t, t.len);
  return (ar >> boost::serialization::make_nvp(t.name.c_str(), sh));
}

/**
 * Catch when we call operator<< with a FirstNormalArrayShim object.  In this
 * case, we make the second-level array shim and use it.  Note that this
 * second-level shim can be used as an lvalue, which is necessary if we want to
 * use make_nvp() safely.  The first-level shim can't be an lvalue (this is why
 * we need two levels of shims).
 */
template<typename Archive, typename T>
Archive& operator<<(Archive& ar, FirstNormalArrayShim<T> t)
{
  SecondNormalArrayShim<T> sh(t.t, t.len);
  return (ar << boost::serialization::make_nvp(t.name.c_str(), sh));
}

/**
 * Catch when we call operator& with a FirstNormalArrayShim object.  In this
 * case, we make the second-level array shim and use it.  Note that this
 * second-level shim can be used as an lvalue, which is necessary if we want to
 * use make_nvp() safely.  The first-level shim can't be an lvalue (this is why
 * we need two levels of shims).
 */
template<typename Archive, typename T>
Archive& operator&(Archive& ar, FirstNormalArrayShim<T> t)
{
  SecondNormalArrayShim<T> sh(t.t, t.len);
  return (ar & boost::serialization::make_nvp(t.name.c_str(), sh));
}

/**
 * Catch when we call operator>> with a FirstNormalArrayShim object.  In this
 * case, we make the second-level array shim and use it.  Note that this
 * second-level shim can be used as an lvalue, which is necessary if we want to
 * use make_nvp() safely.  The first-level shim can't be an lvalue (this is why
 * we need two levels of shims).
 */
template<typename Archive, typename T>
Archive& operator>>(Archive& ar, FirstNormalArrayShim<T> t)
{
  SecondNormalArrayShim<T> sh(t.t, t.len);
  return (ar >> boost::serialization::make_nvp(t.name.c_str(), sh));
}

} // namespace data 
} // namespace mlpack

namespace boost {
namespace serialization {

/**
 * Catch a call to serialize() with a PointerShim, and call the Serialize()
 * function directly.
 */
template<typename Archive, typename T>
inline void serialize(Archive& ar,
                      mlpack::data::PointerShim<T>& t,
                      const BOOST_PFTO unsigned int version)
{
  T* tptr = reinterpret_cast<T*>(&t);
  tptr->Serialize(ar, version);
}

} // namespace serialization
} // namespace boost

#endif
