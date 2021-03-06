/** @file This file contains some C++ template code which makes it easy
    to implement the visitor pattern. 
    
  @author Ralph Tandetzky
  @date 29 Jul 2013 */

#include <cassert>
#include <memory>

/**********************************
 **  THE SIMPLE VISITOR PATTERN  **
 **********************************/

template <typename ...T>
struct Visitor : virtual Visitor<T>...
{
	typedef Visitor<const T...> ConstVisitor;

    template <typename F>
    struct Impl : Visitor, Visitor<T>::template Impl<F>...
    {
        template <typename...Args>
        explicit Impl( Args&&...args ) : f(std::forward<Args>(args)...) {}
        Impl( Impl & other ) : Impl( const_cast<const Impl&>(other) ) {}
        Impl( Impl && ) = default;
        Impl( const Impl & ) = default;
        Impl( const Impl && other ) : Impl( static_cast<const Impl&>(other) ) {}
    private:
        F & getFunctor() override { return f; }
        F f;
    };

    template <typename F>
    static Impl<F> makeImpl( F f )
    {
        return Impl<F>( std::move(f) );
    }

    struct VisitableInterface
    {
        virtual ~VisitableInterface() {}
        virtual void accept( Visitor & v ) = 0;
        virtual void accept( ConstVisitor & v ) const = 0;
    };

    template <typename S>
    struct Visitable : VisitableInterface
    {
        virtual void accept( Visitor & v )
        {
            assert( typeid( *this ) == typeid( S ) );
            static_cast<Visitor<S>&>(v).visit(
                static_cast<S&>(*this) );
        }

        virtual void accept( ConstVisitor & v ) const
        {
            assert( typeid( *this ) == typeid( const S ) );
            static_cast<Visitor<const S>&>(v).visit(
                static_cast<const S&>(*this) );
        }
    };
};

template <typename T>
struct Visitor<T>
{
	virtual ~Visitor() {}
	virtual void visit( T & t ) = 0;

protected:
    template <typename F>
    struct Impl : virtual Visitor<T>
    {
        void visit( T & t ) override { getFunctor()( t ); }
    private:
        virtual F & getFunctor() = 0;
    };
};


/***********************************
 **  THE ACYCLIC VISITOR PATTERN  **
 ***********************************/

struct AcyclicVisitorInterface
{
    virtual ~AcyclicVisitorInterface() {}
};

template <typename ...T>
struct AcyclicVisitor : virtual AcyclicVisitor<T>...
{
    typedef AcyclicVisitor<const T...> ConstVisitor;

    template <typename F>
    struct Impl: AcyclicVisitor<T>::template Impl<F>...
    {
        template <typename...Args>
        Impl( Args&&...args ) : f(std::forward<Args>(args)...) {}
        Impl( Impl & other ) : Impl( const_cast<const Impl&>(other) ) {}
        Impl( Impl && ) = default;
        Impl( const Impl & ) = default;
        Impl( const Impl && other ) : Impl( static_cast<const Impl&>(other) ) {}
    private:
        F & getFunctor() override { return f; }
        F f;
    };

    template <typename F>
    static Impl<F> makeImpl( F f )
    {
        return Impl<F>( std::move(f) );
    }
};

template <typename T>
struct AcyclicVisitor<T> : virtual AcyclicVisitorInterface
{
    virtual void visit( T & ) = 0;

    template <typename F>
    struct Impl : virtual AcyclicVisitor
    {
        void visit( T & t ) override { getFunctor()(t); }
    private:
        virtual F & getFunctor() = 0;
    };
};

struct AcyclicVisitableInterface
{
    virtual ~AcyclicVisitableInterface() {}
    virtual bool tryAccept     ( AcyclicVisitorInterface & v )       = 0;
    virtual bool tryAcceptConst( AcyclicVisitorInterface & v ) const = 0;
};

template <typename S>
struct AcyclicVisitable : virtual AcyclicVisitableInterface
{
    virtual bool tryAccept( AcyclicVisitorInterface & v )
	{
        const auto pv = dynamic_cast<AcyclicVisitor<S>*>(&v);
        if ( pv )
        {
            pv->visit( static_cast<S&>(*this) );
            return true;
        }
        return false;
    }

    virtual bool tryAcceptConst( AcyclicVisitorInterface & v ) const
	{
        const auto pv = dynamic_cast<AcyclicVisitor<const S>*>(&v);
        if ( pv )
        {
            pv->visit( static_cast<const S&>(*this) );
            return true;
        }
        return false;
	}
};


/***********************
 **  GENERIC CLONING  **
 ***********************/

template <typename Base>
struct Cloner
{
    template <typename T>
    void operator()( const T & t )
    { copy = std::unique_ptr<Base>( new T(t) ); }

    std::unique_ptr<Base> copy;
};

template <typename V, typename Base = typename V::VisitableInterface>
std::unique_ptr<Base> clone( const typename V::VisitableInterface & client )
{
    Cloner<Base> cloner;
    typename V::ConstVisitor::template Impl<Cloner<Base>&> v( cloner );
    client.accept( v );
    return std::move(cloner.copy);
}

template <typename V, typename Base>
std::unique_ptr<Base> clone( const AcyclicVisitableInterface & client )
{
    Cloner<Base> cloner;
    typename V::ConstVisitor::template Impl<Cloner<Base>&> v( cloner );
    client.tryAcceptConst( v );
    return std::move(cloner.copy);
}


/*************************
 **  GENERIC STREAMING  **
 *************************/

template <typename OutputStream>
struct Streamer
{
    Streamer( OutputStream & os ) : os(os) {}

    template <typename T>
    void operator()( const T & t ) { os << t; }

private:
    OutputStream & os;
};

template <typename V, typename OutputStream>
OutputStream & print( OutputStream & os, const typename V::VisitableInterface & client )
{
    auto v = V::ConstVisitor::makeImpl( Streamer<OutputStream>(os) );
    client.accept( v );
    return os;
}

template <typename V, typename OutputStream>
OutputStream & print( OutputStream & os, const AcyclicVisitableInterface & client )
{
    auto v = V::ConstVisitor::makeImpl( Streamer<OutputStream>(os) );
    client.tryAcceptConst( v );
    return os;
}
