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


#ifndef __LIBMARY__INTRUSIVE_LIST__H__
#define __LIBMARY__INTRUSIVE_LIST__H__


#include <libmary/types.h>
#include <libmary/intrusive_list.h>


namespace M {

class IntrusiveList_DefaultList;

template <class D = IntrusiveList_DefaultList>
class IntrusiveListElement
{
    template <class T, class Element> friend class IntrusiveList;
    template <class T, class Element> friend class IntrusiveCircularList;

private:
    IntrusiveListElement *next;
    IntrusiveListElement *previous;
};

template <class T, class ListName = IntrusiveList_DefaultList >
class IntrusiveList
{
private:
    typedef IntrusiveListElement<ListName> Element;

    Element *first;
    Element *last;

    static T* objForElement (Element * const mt_nonnull el)
    {
	return static_cast <T*> (el);
    }

    static Element* elementForObj (T * const mt_nonnull obj)
    {
	return static_cast <Element*> (obj);
    }

public:
    T* getFirst () const
    {
	return objForElement (first);
    }

    T* getLast () const
    {
	return objForElement (last);
    }

    static T* getNext (Element * const mt_nonnull obj)
    {
	return objForElement (obj->next);
    }

    static T* getPrevious (Element * const mt_nonnull obj)
    {
	return objForElement (obj->previous);
    }

    bool isEmpty () const
    {
	return first == NULL;
    }

    void append (Element * const mt_nonnull obj)
    {
	append (obj, getLast ());
    }

    void append (Element * const mt_nonnull el,
		 Element * const to_el)
    {
	if (mt_likely (to_el)) {
	    el->next = to_el->next;
	    el->previous = to_el;

	    if (to_el->next != NULL)
		to_el->next->previous = el;

	    to_el->next = el;

	    if (to_el == last)
		last = el;
	} else {
	    el->next = NULL;
	    el->previous = NULL;
	    first = el;
	    last  = el;
	}
    }

    void prepend (Element * const mt_nonnull el)
    {
	prepend (el, getFirst ());
    }

    void prepend (Element * const mt_nonnull el,
		  Element * const to_el)
    {
	if (mt_likely (!to_el)) {
	    el->next = NULL;
	    el->previous = NULL;
	    first = el;
	    last  = el;
	} else {
	    el->previous = to_el->previous;
	    el->next = to_el;

	    if (to_el->previous != NULL)
		to_el->previous->next = el;

	    to_el->previous = el;

	    if (to_el == first)
		first = el;
	}
    }

    void stealAppend (Element * const tosteal_first,
		      Element * const tosteal_last)
    {
	stealAppend (tosteal_first, tosteal_last, last);
    }

    // Remember that source list gets corrupted and needs manual fixup
    // after calling this method.
    void stealAppend (Element * const tosteal_first,
		      Element * const tosteal_last,
		      Element * const to_el)
    {
	if (!tosteal_first)
	    return;

	if (!to_el) {
	    tosteal_first->previous = NULL;
	    first = tosteal_first;
	    last = tosteal_last;
	    return;
	}

	if (to_el->next) {
	    to_el->next->previous = tosteal_last;
	    tosteal_last->next = to_el->next;
	} else {
	    last = tosteal_last;
	}

	to_el->next = tosteal_first;
	tosteal_first->previous = to_el;
    }

    void remove (Element * const mt_nonnull el)
    {
	if (mt_likely (el != first))
	    el->previous->next = el->next;
	else
	    first = el->next;

	if (mt_likely (el != last))
	    el->next->previous = el->previous;
	else
	    last = el->previous;
    }

    void clear ()
    {
	first = NULL;
	last = NULL;
    }

    IntrusiveList ()
	: first (NULL),
	  last (NULL)
    {
    }

// _________________________________ Iterator __________________________________

    class iter
    {
	friend class IntrusiveList;

    private:
	T *cur;

	iter (T * const el) : cur (el) {}

    public:
	iter () {}
	iter (IntrusiveList &list) { list.iter_begin (*this); }

	bool operator == (iter const &iter) const
	{
	    return cur == iter.cur;
	}

	bool operator != (iter const &iter) const
	{
	    return cur != iter.cur;
	}

 	// Methods for C API binding.
	void *getAsVoidPtr () const { return static_cast <void*> (cur); }
	static iter fromVoidPtr (void *ptr) {
		return iter (static_cast <T*> (ptr)); }
    };

    void iter_begin (iter &iter) const
    {
	iter.cur = getFirst ();
    }

    T* iter_next (iter &iter) const
    {
	T * const obj = iter.cur;
	iter.cur = getNext (iter.cur);
	return obj;
    }

    bool iter_done (iter &iter) const
    {
	return iter.cur == NULL;
    }
};

// Note that IntrusiveCircularList object contains just one pointer, while
// IntrusiveList carries two pointers.
template < class T, class Element = IntrusiveListElement<> >
class IntrusiveCircularList
{
private:
    Element *first;

    // TODO It'd be better to use static_cast directly.
    static T* objForElement (Element * const mt_nonnull el)
    {
	return static_cast <T*> (el);
    }

    // TODO It'd be better to use static_cast directly.
    // TODO Use Element* as method arguments as in IntrusiveList.
    static Element* elementForObj (T * const mt_nonnull obj)
    {
	return static_cast <Element*> (obj);
    }

public:
    T* getFirst () const
    {
	return objForElement (first);
    }

    static T* getNext (T * const mt_nonnull obj)
    {
	return objForElement (elementForObj (obj)->next);
    }

    static T* getPrevious (T * const mt_nonnull obj)
    {
	return objForElement (elementForObj (obj)->previous);
    }

    bool isEmpty () const
    {
	return first == NULL;
    }

    void append (T * const mt_nonnull obj)
    {
	Element * const el = elementForObj (obj);

	if (mt_likely (first)) {
	    el->previous = first->previous;
	    first->previous->next = el;

	    el->next = first;
	    first->previous = el;
	} else {
	    el->next = el;
	    el->previous = el;
	    first = el;
	}
    }

    void append (T * const mt_nonnull obj,
		 T * const to_obj)
    {
	Element * const el = elementForObj (obj);

	if (mt_likely (to_obj)) {
	    Element * const to_el = elementForObj (to_obj);

	    el->next = to_el->next;
	    to_el->next->previous = el;

	    el->previous = to_el;
	    to_el->next = el;
	} else {
	    el->next = el;
	    el->previous = el;
	    first = el;
	}
    }

    void prepend (T * const mt_nonnull obj)
    {
	// TODO Calling getFirst() is probably not very effective, because
	// it implies an unnecessary objForElement/elementForObj call pair.
	// The same applies to class IntrusiveList.
	prepend (obj, getFirst ());
    }

    void prepend (T * const mt_nonnull obj,
		  T * const to_obj)
    {
	Element * const el = elementForObj (obj);

	if (mt_likely (!to_obj)) {
	    el->next = el;
	    el->previous = el;
	    first = el;
	} else {
	    Element * const to_el = elementForObj (to_obj);

	    el->previous = to_el->previous;
	    to_el->previous->next = el;

	    el->next = to_el;
	    to_el->previous = el;

	    if (first == to_el)
		first = el;
	}
    }

    void remove (T * const mt_nonnull obj)
    {
	Element * const el = elementForObj (obj);

	if (mt_likely (el->next != el)) {
	    el->previous->next = el->next;
	    el->next->previous = el->previous;

	    if (el == first)
		first = el->next;
	} else {
	    first = NULL;
	}
    }

    void clear ()
    {
	first = NULL;
    }

    IntrusiveCircularList ()
	: first (NULL)
    {
    }
};

}


#endif /* __LIBMARY__INTRUSIVE_LIST__H__ */

