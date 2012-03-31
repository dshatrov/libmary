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


#ifndef __LIBMARY__PAGE_POOL__H__
#define __LIBMARY__PAGE_POOL__H__


#include <libmary/types.h>
#include <libmary/array.h>
#include <libmary/atomic.h>
#include <libmary/mutex.h>
#include <libmary/output_stream.h>


namespace M {

class PagePool
{
public:
    class Page
    {
	friend class PagePool;

    private:
	AtomicInt refcount;

	Page *next_pool_page;
	Page *next_msg_page;

	Page& operator = (Page const &);
	Page (Page const &);

	Page (int refcount = 1)
	    : refcount (refcount)
	{
	}

    public:
	Size data_len;

	Page* getNextMsgPage () const
	{
	    return next_msg_page;
	}

	Byte* getData () const
	{
	    return (Byte*) this + sizeof (*this);
	}

	Memory mem () const
	{
	    return Memory (getData (), data_len);
	}
    };

    class PageListHead
    {
    public:
	Page *first;
	Page *last;

	bool isEmpty () const
	{
	    return first == NULL;
	}

	void reset ()
	{
	    first = NULL;
	    last = NULL;
	}

	PageListHead ()
	    : first (NULL),
	      last  (NULL)
	{
	}
    };

    class PageListArray : public Array
    {
    private:
	Page * const first_page;
	Size const data_len;

	Page *cached_page;
	Size cached_pos;

	void doGetSet (Size offset,
		       Byte       * const data_get,
		       Byte const * const data_set,
		       Size data_len,
		       bool get);

    public:
	void get (Size offset,
		  Memory const &mem);

	void set (Size offset,
		  ConstMemory const &mem);

	// TODO Add msg_offset
	PageListArray (Page * const first_page,
		       Size const data_len)
	    : first_page (first_page),
	      data_len (data_len),
	      cached_page (NULL),
	      cached_pos (0)
	{
	}
    };

    class PageListOutputStream : public OutputStream
    {
    private:
	PagePool * const page_pool;
	PageListHead * const page_list;

    public:
      mt_iface (OutputStream)

	mt_throws Result write (ConstMemory   const mem,
				Size        * const ret_nwritten)
	{
	    page_pool->getFillPages (page_list, mem);

	    if (ret_nwritten)
		*ret_nwritten = mem.len();

	    return Result::Success;
	}

	mt_throws Result flush ()
	{
	  // No-op
	    return Result::Success;
	}

      mt_iface_end

	PageListOutputStream (PagePool     * const mt_nonnull page_pool,
			      PageListHead * const mt_nonnull page_list)
	    : page_pool (page_pool),
	      page_list (page_list)
	{
	}
    };

    struct Statistics
    {
	Count num_spare_pages;
	Count num_busy_pages;
    };

private:
    mt_const Size const page_size;
    mt_const Count min_pages;

    Count num_pages;

    Page *first_spare_page;

    Mutex mutex;

    void doGetPages (PageListHead * mt_nonnull page_list,
		     ConstMemory const &mem,
		     bool fill);

public:
    Statistics stats;

    Size getPageSize () const
    {
	return page_size;
    }

    void getFillPages (PageListHead * mt_nonnull page_list,
		       ConstMemory const &mem);

    void getPages (PageListHead * mt_nonnull page_list,
		   Size len);

    void pageRef (Page * mt_nonnull page);

    void pageUnref (Page * mt_nonnull page);

    void msgRef (Page * mt_nonnull first_page);

    void msgUnref (Page * mt_nonnull first_page);

    // printToPages() should never fail.
    template <class ...Args>
    void printToPages (PageListHead * const mt_nonnull page_list, Args const &...args)
    {
	PageListOutputStream pl_outs (this, page_list);
	pl_outs.print (args...);
    }

    void setMinPages (Count min_pages);

    PagePool (Size  page_size,
	      Count min_pages);

    ~PagePool ();

    static void dumpPages (OutputStream * mt_nonnull outs,
                           PageListHead * mt_nonnull page_list);
};

}


#endif /* __LIBMARY__PAGE_POOL__H__ */

