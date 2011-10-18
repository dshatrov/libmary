/*  LibMary - C++ library for high-performance network servers
    Copyright (C) 2011 Dmitry Shatrov

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
    Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/


#ifndef __LIBMARY__STRING_HASH__H__
#define __LIBMARY__STRING_HASH__H__


#include <libmary/hash.h>


namespace M {

template <class T>
class StringHash_anybase
{
private:
    class Entry : public HashEntry<>
    {
    public:
	Ref<String> str;
	T data;

	Entry (ConstMemory const &mem,
	       T data)
	    : str (grab (new String (mem))),
	      data (data)
	{
	}

	Entry (ConstMemory const &mem)
	    : str (grab (new String (mem)))
	{
	}
    };

    typedef Hash< Entry,
		  ConstMemory,
		  MemberExtractor< Entry,
				   Ref<String>,
				   &Entry::str,
				   ConstMemory,
				   AccessorExtractor< String,
						      Memory,
						      &String::mem > >,
		  MemoryComparator<>,
		  DefaultStringHasher >
	    StrHash;

    StrHash hash;

public:
    class EntryKey
    {
	friend class StringHash_anybase;
    private:
	Entry *entry;
	EntryKey (Entry * const entry) : entry (entry) {}
    public:
	operator bool () const { return entry; }
	T getData () const { return entry->data; }
	T* getDataPtr() const { return &entry->data; }
	EntryKey () : entry (NULL) {}

	// Methods for C API binding.
	void *getAsVoidPtr () const { return static_cast <void*> (entry); }
	static EntryKey fromVoidPtr (void *ptr) { return EntryKey (static_cast <Entry*> (ptr)); }
    };

    bool isEmpty () const
    {
	return hash.isEmpty ();
    }

    EntryKey add (ConstMemory const &mem,
		  T data)
    {
	Entry * const entry = new Entry (mem, data);
	hash.add (entry);
	return entry;
    }

    EntryKey addEmpty (ConstMemory const &mem)
    {
	Entry * const entry = new Entry (mem);
	hash.add (entry);
	return entry;
    }

    void remove (EntryKey const key)
    {
	hash.remove (key.entry);
	delete key.entry;
    }

    template <class C>
    EntryKey lookup (C key)
    {
	return hash.lookup (key);
    }

    StringHash_anybase (Size const initial_hash_size,
			bool const growing)
	: hash (initial_hash_size, growing)
    {
    }

    ~StringHash_anybase ()
    {
	typename StrHash::iter iter (hash);
	while (!hash.iter_done (iter)) {
	    Entry * const entry = hash.iter_next (iter);
	    delete entry;
	}
    }

  // Iterators

    class iter
    {
	friend class StringHash_anybase;
    private:
	typename StrHash::iter iter;
    public:
	iter () {}
	iter (StringHash_anybase &hash) : iter (hash.hash) {}
    };

    void iter_begin (iter &iter) const
    {
	hash.iter_begin (iter.iter);
    }

    EntryKey iter_next (iter &iter) const
    {
	return hash.iter_next (iter.iter);
    }

    bool iter_done (iter &iter) const
    {
	return hash.iter_done (iter.iter);
    }
};

template < class T, class Base = EmptyBase >
class StringHash : public StringHash_anybase<T>,
		   public Base
{
public:
    StringHash (Size const initial_hash_size = 16,
		bool const growing = true)
	: StringHash_anybase<T> (initial_hash_size, growing)
    {
    }
};

}


#endif /* __LIBMARY__STRING_HASH__H__ */

