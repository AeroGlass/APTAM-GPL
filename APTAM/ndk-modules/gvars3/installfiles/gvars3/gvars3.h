/*                       
	This file is part of the GVars3 Library.

	Copyright (C) 2005 The Authors

	This library is free software; you can redistribute it and/or
	modify it under the terms of the GNU Lesser General Public
	License as published by the Free Software Foundation; either
	version 2.1 of the License, or (at your option) any later version.

	This library is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
	Lesser General Public License for more details.

	You should have received a copy of the GNU Lesser General Public
	License along with this library; if not, write to the Free Software
	Foundation, Inc., 
    51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
	
	modified by ICGJKU 2015
*/

#ifndef GV3_INV_GVARS3_H
#define GV3_INV_GVARS3_H
#include <map>
#include <set>
#include <string>
#include <list>
#include <vector>
#include <iostream>

#include <gvars3/config.h>
#include <gvars3/default.h>
#include <gvars3/type_name.h>
#include <gvars3/serialize.h>

#define GV_THREADSAFE
#ifdef GV_THREADSAFE
#include <pthread.h>
#endif

namespace GVars3
{
void parse_warning(int e, std::string type, std::string name, std::string from);

struct type_mismatch{};
struct gvar_was_not_defined{};


class GV3;

#ifdef GV_THREADSAFE

class GVMutex
{
public:
	GVMutex()
  {
    pthread_mutex_init(&m_lock, NULL);
    is_locked = false;
  }
  ~GVMutex()
  {
    if (is_locked) {
      unlock(); // FIXME: is this correct? Can a thread unlock a mutex locked by another thread?
    }
    pthread_mutex_destroy(&m_lock);
  }
  void lock()
  {
    pthread_mutex_lock(&m_lock);
    is_locked = true;
  }
  void unlock()
  {
    is_locked = false; // do it BEFORE unlocking to avoid race condition
    pthread_mutex_unlock(&m_lock);
  }
  pthread_mutex_t* get_mutex_ptr()
  {
    return &m_lock;
  }
private:
  pthread_mutex_t m_lock;
  volatile bool is_locked;
};

extern GVMutex globallock;
#endif


class BaseMap
{
	public:
		virtual std::string get_as_string(const std::string& name, bool precise)=0;
		virtual int set_from_string(const std::string& name, const std::string& val)=0;
		virtual std::string name()=0;
		virtual std::vector<std::string> list_tags()=0;
		virtual ~BaseMap(){};
};

template<class T> class gvar2
{
	friend class GV3;
	public:
		gvar2() {data=NULL;}      // Debugging comfort feature, makes unregistered vars more obvious.
		T& operator*()
		{
			return data->get();
		}

		const T & operator*() const 
		{
			return data->get();
		}

		T* operator->()
		{
			return data->ptr();
		}

		const T * operator->() const 
		{
			return data->ptr();
		}

		bool IsRegistered() const
		{
			return data!=NULL;
		}

	protected:
		ValueHolder<T>* data;
};

// Bit-masks for gvar registration:
// SILENT makes gvars not complain if it has to use the default;
// HIDDEN makes vars not appear in gvarlist unless used with -a

enum { SILENT = 1<<0, HIDDEN = 1<<1, FATAL_IF_NOT_DEFINED = 1<<2};
       

typedef gvar2<double> gvar2_double;
typedef gvar2<int> gvar2_int;
typedef gvar2<std::string> gvar2_string;

template<class T> class gvar3: public gvar2<T>
{
	friend class GV3;
	public:
	inline gvar3(const std::string& name, const T& default_val = T(), int flags=0);
	inline gvar3(const std::string& name, const std::string& default_val, int flags);
	inline gvar3(){};
};

template<> class gvar3<std::string>: public gvar2<std::string>
{
	friend class GV3;
	public:
	inline gvar3(const std::string& name, const std::string& default_val = "", int flags=0);
	inline gvar3(){};
};
class GV3
{
	private:

		template<class T> class TypedMap: public BaseMap
		{
			private:
				friend class GV3;

				//This gives us singletons
				static TypedMap& instance()
				{
					static TypedMap* inst=0;

#ifdef GV_THREADSAFE
					globallock.lock();
#endif
					if(!inst)
					{
						inst = new TypedMap();
						//Register ourselves with GV3
						GV3::add_typemap(inst);
					}
#ifdef GV_THREADSAFE
					globallock.unlock();
#endif

					return *inst;
				}

				//Get a data member	
				ValueHolder<T>* get(const std::string& n)
				{
#ifdef GV_THREADSAFE
					globallock.lock();

					ValueHolder<T>* rval = NULL;
					DataIter i;

					i = data.find(n);

					if(i != data.end())
						rval = &(i->second);

					globallock.unlock();

					return rval;
#else
					DataIter i;

					i = data.find(n);

					if(i == data.end())
						return NULL;
					else
						return &(i->second);
#endif
				}
				
				ValueHolder<T>* safe_replace(const std::string& n, const T& t)
				{
#ifdef GV_THREADSAFE
					ValueHolder<T>* ret = NULL;

					globallock.lock();
					DataIter i, j;
					//Keep track of the neighboring point
					//to pass as a hint to insert.
					i = data.find(n);

					if(i == data.end())
					{
						ret = &(data.insert(make_pair(n, t)).first->second);

					}
					else
					{
						i->second.set(t);
						ret = &(i->second);
					}
					globallock.unlock();

					return ret;
#else
					DataIter i, j;
					//Keep track of the neighboring point
					//to pass as a hint to insert.
					i = data.find(n);

					if(i == data.end())
					{
						return &(data.insert(make_pair(n, t)).first->second);

					}
					else
					{
						i->second.set(t);
						return &(i->second);
					}
#endif

				}

				//Create a data member
				ValueHolder<T>* create(const std::string& n)
				{
#ifdef GV_THREADSAFE
					ValueHolder<T>* ret = NULL;
					globallock.lock();
					ret = &(data.insert(make_pair(n, DefaultValue<T>::val()))->second);
					globallock.unlock();
					return ret;
#else
					return &(data.insert(make_pair(n, DefaultValue<T>::val()))->second);
#endif
				}
			
				virtual int set_from_string(const std::string& name, const std::string& val)
				{
					std::string copystr = val + "\n\n";
					std::istringstream is(copystr);
					T tmp = serialize::from_stream<T>(is);
					int e = serialize::check_stream(is);

					if(e == 0)
						safe_replace(name, tmp);
					return e;
				}

				virtual std::string get_as_string(const std::string& name, bool precise)
				{	
#ifdef GV_THREADSAFE
					std::string ret;
					globallock.lock();
					DataIter i = data.find(name);

					if(i == data.end())
						i = data.insert(make_pair(name, DefaultValue<T>::val())).first;

					ret = serialize::to_string(i->second.get(), precise);
					globallock.unlock();
					return ret;
#else
					DataIter i = data.find(name);

					if(i == data.end())
						i = data.insert(make_pair(name, DefaultValue<T>::val())).first;

					return serialize::to_string(i->second.get(), precise);
#endif
				}

				virtual std::string name()
				{
					return type_name<T>();
				}

				virtual std::vector<std::string> list_tags()
				{
					std::vector<std::string> l;
#ifdef GV_THREADSAFE
					globallock.lock();
#endif
					for(DataIter i=data.begin(); i != data.end(); i++)
						l.push_back(i->first);
#ifdef GV_THREADSAFE
					globallock.unlock();
#endif
					return l;
				}

				std::map<std::string, ValueHolder<T> >		data;
				typedef typename std::map<std::string, ValueHolder<T> >::iterator DataIter;

		};

		template<class T> friend class TypedMap;

		template<class T> static ValueHolder<T>* attempt_get(const std::string& name)
		{
			ValueHolder<T>* d = TypedMap<T>::instance().get(name);
			
			if(!d)	 //Data not present in map of the correct type
			{
				//Does it exist with a different type?
				if(registered_type_and_trait.count(name))
				{		//Yes: programmer error.
					std::cerr << "GV3:Error: type mismatch while getting " << type_name<T>() << " " << name << ": already registered "
							"as type " << registered_type_and_trait[name].first->name() << ". Fix your code.\n";

					throw type_mismatch();
				}
				else
					return NULL;
			}

			return d;
		}

		template<class T> static ValueHolder<T>* safe_replace(const std::string& name, const T& t)
		{
			return TypedMap<T>::instance().safe_replace(name, t);
		}

		static void add_typemap(BaseMap* m);

		static std::map<std::string, std::string>		unmatched_tags;
		static std::map<std::string, std::pair<BaseMap*, int> >	registered_type_and_trait;
		static std::list<BaseMap*>				maps;

		
		template<class T> static ValueHolder<T>* get_by_val(const std::string& name, const T& default_val, int flags);
		template<class T> static ValueHolder<T>* get_by_str(const std::string& name, const std::string& default_val, int flags);
		template<class T> static ValueHolder<T>* register_new_gvar(const std::string &name, const T& default_val, int flags);

	public:
		//Get references by name
		template<class T> static T& get(const std::string& name, const T& default_val = defaultValue<T>(), int flags=0);
		template<class T> static T& get(const std::string& name, std::string default_val, int flags=0);
		
		//Register GVars
		template<class T> static void Register(gvar2<T>& gv, const std::string& name, const T& default_val=defaultValue<T>(), int flags=0);
		template<class T> static void Register(gvar2<T>& gv, const std::string& name, const std::string& default_val, int flags=0);
		static inline void Register(gvar2<std::string>& gv, const std::string& name, const std::string& default_val="", int flags=0);

		//Get and set by string only
		static std::string get_var(std::string name);
		static bool set_var(std::string name, std::string val, bool silent=false);

		//Some helper functions
		static void print_var_list(std::ostream& o, std::string pattern="", bool show_all=true);
		static std::vector<std::string> tag_list();
};



template<class T> gvar3<T>::gvar3(const std::string& name, const T& default_val, int flags)
{
	GV3::Register(*this, name, default_val, flags);
}

template<class T> gvar3<T>::gvar3(const std::string& name, const std::string& default_val, int flags)
{
	GV3::Register(*this, name, default_val, flags);
}
gvar3<std::string>::gvar3(const std::string& name, const std::string& default_val, int flags)
{
	GV3::Register(*this, name, default_val, flags);
}


#include <gvars3/gv3_implementation.hh>

//Compatibility with old GVARS
class GVars2
{
	public:
		template<class T> void Register(gvar2<T>& gv, const std::string& name, const T& default_val=T(), int flags=false)
		{ 
			GV3::Register(gv, name, default_val, flags);
		}

		template<class T> void Register(gvar2<T>& gv, const std::string& name, const std::string& default_val, int flags=false)
		{
			GV3::Register(gv, name, default_val, flags);
		}

		inline void Register(gvar2<std::string>& gv, const std::string& name, const std::string& default_val="", int flags=false)
		{
			GV3::Register(gv, name, default_val, flags);
		}

		template<class T> T& Get(const std::string& name, const T& default_val=T(),  int flags=false)
		{
			return GV3::get<T>(name, default_val, flags);
		}

		template<class T> T& Get(const std::string& name, const std::string& default_val="", int flags=false)
		{
			return GV3::get<T>(name, default_val, flags);
		}

		inline std::string& Get(const std::string& name, const std::string& default_val="", int flags=false)
		{
			return GV3::get<std::string>(name, default_val, flags);
		}

		void SetVar(std::string sVar, std::string sValue, bool silent=false);
		void SetVar(std::string s);


		int& GetInt(const std::string& name, int default_val=0, int flags=0);
		double& GetDouble(const std::string& name, double default_val=0.0, int flags=0); 
		std::string& GetString(const std::string& name, const std::string& default_val="", int flags=0); 

		int& GetInt(const std::string& name, const std::string& default_val, int flags=0);
		double& GetDouble(const std::string& name, const std::string& default_val, int flags=0); 

		std::string StringValue(const std::string &name, bool no_quotes=false);
		void PrintVarList(std::ostream& os=std::cout);
		void PrintVar(std::string s, std::ostream& os, bool bEndl = true);

		//char* ReadlineCommandGenerator(const char *szText, int nState);

	private:
};

}
#endif
