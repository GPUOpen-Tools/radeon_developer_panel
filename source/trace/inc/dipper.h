// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief Declaration for simple dependency injection.

#ifndef RDP_SOURCE_TRACE_INC_DIPPER_H_
#define RDP_SOURCE_TRACE_INC_DIPPER_H_

#include <functional>
#include <memory>
#include <type_traits>
#include <unordered_map>

#include <dd_driver_utils_api.h>

#include <dev_trace_common.h>

#define DIP(sig)                \
    using DipConstructor = sig; \
    explicit sig

namespace dipper
{
    struct Resolver;

    /// @brief Interface for an untyped factory.
    struct BaseFactory
    {
        /// @brief Destructor.
        virtual ~BaseFactory() = default;

        /// @brief Copies this factory.
        /// @return The copy of this factory.
        virtual std::unique_ptr<BaseFactory> Copy() const = 0;
    };

    /// @brief Factory that can produce a specific type.
    /// @tparam Reg The type the factory was registered as and what this factory produces.
    template <typename Reg>
    struct Factory : public BaseFactory
    {
        /// @brief Destructor.
        ~Factory() override = default;

        /// @brief Makes an object.
        /// @param [in] resolver The resolver to use to get dependencies.
        /// @return A newly created object.
        virtual Reg Make(const Resolver& resolver) const = 0;
    };

    /// @brief Resolves registered dependencies.
    struct Resolver
    {
        /// @brief Storage type for factories, keys are typeid() hashes.
        using Factories = std::unordered_map<size_t, std::unique_ptr<BaseFactory>>;

    public:
        /// @brief Constructor.
        /// @param [in] factories Factories to use to construct resolved objects.
        Resolver(const std::shared_ptr<Factories>& factories);

    private:
        /// @brief Helper to call the regular value factory if T is a const reference.
        /// @tparam T The type of value to resolve.
        template <typename T>
        struct ResolveHelper
        {
            using Ret = T;  ///< Return type of the resolve function.

            /// @brief Resolves an object.
            /// @param [in] resolver The resolver to use.
            /// @return The constructed object.
            static T Resolve(const Resolver& resolver);
        };

        /// @brief Template specialization to construct regular value when T is a const reference.
        /// @tparam T The type of value to resolve.
        template <typename T>
        struct ResolveHelper<const T&>
        {
            using Ret = T;  ///< Return type of the resolve function.

            /// @brief Resolves an object.
            /// @param [in] resolver The resolver to use.
            /// @return The constructed object.
            static T Resolve(const Resolver& resolver);
        };

    public:
        /// @brief Resolves an object.
        /// @return The constructed object.
        template <typename T>
        typename ResolveHelper<T>::Ret Resolve() const;

    protected:
        std::shared_ptr<Factories> factories_;  ///< The factories to use to construct objects.
    };

    template <typename T>
    T Resolver::ResolveHelper<T>::Resolve(const Resolver& resolver)
    {
        size_t hash = typeid(T).hash_code();
        DEV_TRACE_ASSERT_MSG(resolver.factories_->count(hash) != 0, "No factory found for type " << typeid(T).name());

        Factory<T>* factory = dynamic_cast<Factory<T>*>(resolver.factories_->at(hash).get());
        DEV_TRACE_ASSERT_MSG(factory != nullptr, "Factory was not of the correct type");

        return factory->Make(resolver);
    }

    template <typename T>
    T Resolver::ResolveHelper<const T&>::Resolve(const Resolver& resolver)
    {
        return resolver.Resolve<T>();
    }

    template <typename T>
    typename Resolver::ResolveHelper<T>::Ret Resolver::Resolve() const
    {
        return ResolveHelper<T>::Resolve(*this);
    }

    /// @brief Factory that can produce a specific type using a function.
    /// @tparam Reg The type the factory was registered as and what this factory produces.
    /// @tparam Args The types of the arguments to resolve and pass to the function.
    template <typename Reg, typename... Args>
    struct FnFactory : public Factory<Reg>
    {
        /// @brief Constructor.
        /// @param [in] fn The function to use to construct objects.
        FnFactory(const std::function<Reg(Args...)>& fn);

        /// @brief Destructor.
        ~FnFactory() override = default;

        Reg Make(const Resolver& resolver) const override;

        std::unique_ptr<BaseFactory> Copy() const override;

    private:
        std::function<Reg(Args...)> fn_;  ///< The function to use to construct objects.
    };

    template <typename Reg, typename... Args>
    FnFactory<Reg, Args...>::FnFactory(const std::function<Reg(Args...)>& fn)
        : fn_(fn)
    {
    }

    template <typename Reg, typename... Args>
    Reg FnFactory<Reg, Args...>::Make(const Resolver& resolver) const
    {
        return fn_(resolver.Resolve<Args>()...);
    }

    template <typename Reg, typename... Args>
    std::unique_ptr<BaseFactory> FnFactory<Reg, Args...>::Copy() const
    {
        return std::make_unique<FnFactory>(fn_);
    }

    /// @brief Factory that produces a singleton.
    /// @tparam Reg The type the factory was registered as and what this factory produces.
    /// @tparam Args The types of the arguments to resolve and pass to the function.
    template <typename Reg, typename... Args>
    struct SingletonFactory : public Factory<std::shared_ptr<Reg>>
    {
        /// @brief Constructor.
        /// @param [in] fn The function to use to construct the singleton.
        SingletonFactory(const std::function<std::shared_ptr<Reg>(Args...)>& fn);

    private:
        /// @brief Copy constructor.
        /// @param [in] other The factory to copy.
        SingletonFactory(const SingletonFactory& other);

    public:
        /// @brief Destructor.
        ~SingletonFactory() override = default;

        std::shared_ptr<Reg>         Make(const Resolver& resolver) const override;
        std::unique_ptr<BaseFactory> Copy() const override;

    private:
        std::function<std::shared_ptr<Reg>(Args...)> fn_;     ///< The function to use to construct objects.
        mutable std::shared_ptr<Reg>                 value_;  ///< The singleton.
    };

    template <typename Reg, typename... Args>
    SingletonFactory<Reg, Args...>::SingletonFactory(const std::function<std::shared_ptr<Reg>(Args...)>& fn)
        : fn_(fn)
    {
    }

    template <typename Reg, typename... Args>
    SingletonFactory<Reg, Args...>::SingletonFactory(const SingletonFactory& other)
        : fn_(other.fn_)
        , value_(other.value_)
    {
    }

    template <typename Reg, typename... Args>
    std::shared_ptr<Reg> SingletonFactory<Reg, Args...>::Make(const Resolver& resolver) const
    {
        if (value_ == nullptr)
        {
            value_ = fn_(resolver.Resolve<Args>()...);
        }

        return value_;
    }

    template <typename Reg, typename... Args>
    std::unique_ptr<BaseFactory> SingletonFactory<Reg, Args...>::Copy() const
    {
        return std::unique_ptr<SingletonFactory>(new SingletonFactory(*this));
    }

    /// @brief Allows binding dependencies and getting a resolver to resolve them.
    struct Container
    {
    private:
        /// @brief Helper to extract return type and args from a function type.
        /// @tparam [in] Reg The type to register in the container.
        /// @tparam [in] Impl The type of the implementation.
        template <typename Reg, typename Impl>
        struct RegisterHelper;

        /// @brief Template specialization that extracts return type and args from a function type.
        /// @tparam Reg The type to register in the container.
        /// @tparam Impl The type of the implementation.
        /// @tparam Args The types for the arguments of the constructor of Impl.
        template <typename Reg, typename Impl, typename... Args>
        struct RegisterHelper<Reg, Impl(Args...)>
        {
            static_assert(!std::is_pointer_v<Reg>, "Registration type must not be a pointer type.");
            static_assert(!std::is_pointer_v<Impl>, "Implementation type must not be a pointer type.");

            /// @brief Registers Impl in the container by calling the constructor Impl(Args...).
            /// @param [in] container The container to register in.
            static void Register(Container& container);

            /// @brief Registers a singleton Impl in the container by calling the constructor Impl(Args...).
            /// @param [in] container The container to register in.
            static void RegisterSingleton(Container& container);
        };

    public:
        /// @brief Constructor.
        Container();

        /// @brief Registers the implementation for the type.
        /// @tparam Reg The type to register Impl for.
        /// @tparam Impl The implementation to constructor for Reg.
        template <typename Reg, typename Impl>
        void Register();

        /// @brief Registers the a singleton of the implementation for the type.
        /// @tparam Reg The type to register Impl for.
        /// @tparam Impl The implementation to constructor for Reg.
        template <typename Reg, typename Impl>
        void RegisterSingleton();

        /// @brief Registers a specific instance to resolve.
        /// @tparam Reg The type of the instance to register.
        /// @param [in] value The value to register
        template <typename Reg>
        void RegisterValue(const Reg& value);

        /// @brief Gets the resolver that will resolve everything registered in this container.
        /// @return The resolver that will resolve everything registered in this container.
        std::shared_ptr<Resolver> GetResolver() const;

        /// @brief Installs another components registered types into this container by copying the factories (instances will be copied).
        /// @param [in] other The other container to install.
        void Install(const Container& other);

    private:
        /// @brief Internal function for registering a factory.
        /// @tparam Reg The type to register the factory for.
        /// @param [in] factory The factory to register.
        template <typename Reg>
        void RegisterInternal(std::unique_ptr<Factory<Reg>> factory);

        std::shared_ptr<Resolver::Factories> factories_ = std::make_shared<Resolver::Factories>();  ///< The factories that have been registered.
        std::shared_ptr<Resolver>            resolver_;                                             ///< The resolver for this container.
    };

    template <typename Reg, typename Impl, typename... Args>
    void Container::RegisterHelper<Reg, Impl(Args...)>::Register(Container& container)
    {
        using Shared = std::shared_ptr<Reg>;
        using Unique = std::unique_ptr<Reg>;

        container.RegisterInternal<Shared>(std::make_unique<FnFactory<Shared, Args...>>([](Args... args) { return std::make_shared<Impl>(args...); }));
        container.RegisterInternal<Unique>(std::make_unique<FnFactory<Unique, Args...>>([](Args... args) { return std::make_unique<Impl>(args...); }));
        // This is potentially unsafe, so we're not going to allow it.
        // container.RegisterInternal<Reg*>(std::make_unique<FnFactory<Reg*, Args...>>([](Args... args) { return new Impl(args...); }));
    }

    template <typename Reg, typename Impl, typename... Args>
    void Container::RegisterHelper<Reg, Impl(Args...)>::RegisterSingleton(Container& container)
    {
        using Shared = std::shared_ptr<Reg>;
        container.RegisterInternal<Shared>(std::make_unique<SingletonFactory<Reg, Args...>>([](Args... args) { return std::make_shared<Impl>(args...); }));
        // This is potentially unsafe, so we're not going to allow it.
        // container.RegisterInternal<Reg*>(std::make_unique<FnFactory<Reg*, Shared>>([](Shared ptr) { return ptr.get(); }));
    }

    template <typename Reg, typename Impl>
    void Container::Register()
    {
        RegisterHelper<Reg, typename Impl::DipConstructor>::Register(*this);
    }

    template <typename Reg, typename Impl>
    void Container::RegisterSingleton()
    {
        RegisterHelper<Reg, typename Impl::DipConstructor>::RegisterSingleton(*this);
    }

    template <typename Reg>
    void Container::RegisterValue(const Reg& value)
    {
        RegisterInternal<Reg>(std::make_unique<FnFactory<Reg>>([=]() { return value; }));
    }

    template <typename Reg>
    void Container::RegisterInternal(std::unique_ptr<Factory<Reg>> factory)
    {
        size_t hash = typeid(Reg).hash_code();
        DEV_TRACE_ASSERT_MSG(factories_->count(hash) == 0, "An existing factory already existed.");

        factories_->insert({hash, std::move(factory)});
    }
}  // namespace dipper

#endif
