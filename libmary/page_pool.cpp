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


#include "local_config.h"

#include <libmary/types.h>
#include <new>

#include <libmary/log.h>

#include <libmary/page_pool.h>


namespace M {

namespace {
LogGroup libMary_logGroup_pool ("pool", LogLevel::N);
}

void
PagePool::PageListArray::doGetSet (Size const offset,
				   Byte       * const data_get,
				   Byte const * const data_set,
				   Size const data_len,
				   bool const get)
{
    assert (offset + data_len >= offset);

    if (data_len == 0)
	return;

    Page *cur_page;
    Size cur_pos;
    if (cached_page &&
	offset >= cached_pos)
    {
	cur_page = cached_page;
	cur_pos = cached_pos;
    } else {
	cur_page = first_page;
	cur_pos = 0;
    }

    Size data_pos = 0;

    for (;;) {
	assert (cur_page);
	assert (cur_pos + cur_page->data_len > cur_pos);

	if (cur_pos + cur_page->data_len <= offset) {
	    cur_pos += cur_page->data_len;
	    cur_page = cur_page->next_msg_page;
	    continue;
	}

	Size copy_from;
	if (cur_pos < offset) {
	    copy_from = offset - cur_pos;
	    assert (copy_from < cur_page->data_len);
	} else {
	    copy_from = 0;
	}

	Size copy_to;
	if (offset + data_len < cur_pos + cur_page->data_len) {
	    copy_to = offset + data_len - cur_pos;
	    assert (copy_to < cur_page->data_len);
	} else {
	    copy_to = cur_page->data_len;
	}

	assert (copy_to > copy_from);
	Size copy_len = copy_to - copy_from;

	if (get)
	    memcpy (data_get + data_pos, cur_page->getData() + copy_from, copy_len);
	else
	    memcpy (cur_page->getData() + copy_from, data_set + data_pos, copy_len);

	data_pos += copy_len;
	assert (data_pos <= data_len);
	if (data_pos == data_len)
	    break;

	cur_pos += cur_page->data_len;
	cur_page = cur_page->next_msg_page;

	cached_page = cur_page;
	cached_pos = cur_pos;
    }
}

void
PagePool::PageListArray::get (Size const offset,
			      Memory const &mem)
{
    doGetSet (offset, mem.mem() /* data_get */, NULL /* data_set */, mem.len(), true /* get */);
}

void
PagePool::PageListArray::set (Size const offset,
			      ConstMemory const &mem)
{
    doGetSet (offset, NULL /* data_get */, mem.mem() /* data_set */, mem.len(), false /* get */);
}

void
PagePool::doGetPages (PageListHead * const mt_nonnull page_list,
		      ConstMemory    const &mem,
		      bool           const  fill)
{
    Byte const *cur_data = mem.mem ();
    Size cur_data_len = mem.len ();

    mutex.lock ();

    if (page_list->last != NULL) {
	Page * const page = page_list->last;
	if (page->data_len < page_size) {
//	    logD (pool, _func, "adding data to page 0x", fmt_hex, (UintPtr) page);

	    Size tocopy = cur_data_len;
	    if (cur_data_len > page_size - page->data_len)
		tocopy = page_size - page->data_len;

	    if (fill)
		memcpy (page->getData() + page->data_len, cur_data, tocopy);

	    page->data_len += tocopy;
	    cur_data += tocopy;
	    cur_data_len -= tocopy;
	}
    }

    while (cur_data_len > 0) {
	Page *page;
	if (!first_spare_page) {
	    // Default page refcount is 1.
	    page = new (new Byte [sizeof (Page) + page_size]) Page;
	    assert (page);

	    logD (pool, _func, "new page 0x", fmt_hex, (UintPtr) page);

	    ++num_pages;
	} else {
	    page = first_spare_page;
	    first_spare_page = first_spare_page->next_pool_page;

	    pageRef (page);

	    logD (pool, _func, "spare page 0x", fmt_hex, (UintPtr) page);

	    assert (stats.num_spare_pages > 0);
	    --stats.num_spare_pages;
	}
	page->next_msg_page = NULL;

	++stats.num_busy_pages;

	{
	  // Dealing with the linked list.

	    if (!page_list->first)
		page_list->first = page;

	    if (page_list->last)
		page_list->last->next_msg_page = page;

	    page_list->last = page;
	}

	Size const tocopy = (cur_data_len <= page_size ? cur_data_len : page_size);

	if (fill)
	    memcpy (page->getData(), cur_data, tocopy);

	page->data_len = tocopy;
	cur_data += tocopy;
	cur_data_len -= tocopy;
    }

    mutex.unlock ();
}

void
PagePool::getFillPages (PageListHead * const mt_nonnull page_list,
			ConstMemory const &mem)
{
    doGetPages (page_list, mem, true /* fill */);
}

void
PagePool::getPages (PageListHead * const mt_nonnull page_list,
		    Size const len)
{
    doGetPages (page_list, ConstMemory ((Byte*) NULL, len), false /* fill */);
}

void
PagePool::pageRef (Page * const mt_nonnull page) 
{
    logD (pool, _func, "0x", fmt_hex, (UintPtr) page, ": ", page->refcount.get());
    page->refcount.inc ();
}

void
PagePool::pageUnref (Page * const mt_nonnull page)
{
    logD (pool, _func, "0x", fmt_hex, (UintPtr) page, ": ", page->refcount.get());

    if (!page->refcount.decAndTest ())
	return;

    mutex.lock ();

    assert (stats.num_busy_pages > 0);
    --stats.num_busy_pages;

    if (num_pages <= min_pages) {
	logD (pool, _func, "num_pages: ", num_pages, ", spare page 0x", fmt_hex, (UintPtr) page);

	page->next_pool_page = first_spare_page;
	first_spare_page = page;
	++stats.num_spare_pages;
    } else {
	logD (pool, _func, "num_pages: ", num_pages, ", freeing page 0x", fmt_hex, (UintPtr) page);

	assert (num_pages > 0);
	--num_pages;
	mutex.unlock ();

	// FIXME mismatch free/delete/delete[] complaints from valgrind.
	page->~Page();
	delete[] (Byte*) page;

	return;
    }

    mutex.unlock ();
}

void
PagePool::msgRef (Page * const mt_nonnull first_page)
{
    logD (pool, _func);

    Page *cur_page = first_page;
    while (cur_page) {
	Page * const next_page = cur_page->next_msg_page;
	pageRef (cur_page);
	cur_page = next_page;
    }
}

void
PagePool::msgUnref (Page * const mt_nonnull first_page)
{
    logD (pool, _func_);

    Page *cur_page = first_page;
    while (cur_page) {
	Page * const next_page = cur_page->next_msg_page;
	pageUnref (cur_page);
	cur_page = next_page;
    }
}

void
PagePool::setMinPages (Count const min_pages)
{
    mutex.lock ();

    if (min_pages >= this->min_pages) {
	while (num_pages > min_pages &&
	       first_spare_page)
	{
	    Page * const page = first_spare_page;
	    first_spare_page = first_spare_page->next_pool_page;

	    // FIXME mismatch free/delete/delete[] complaints from valgrind.
	    page->~Page();
	    delete[] (Byte*) page;

	    assert (stats.num_spare_pages > 0);
	    --stats.num_spare_pages;
	    --num_pages;
	}
    } else {
	while (num_pages < min_pages) {
	    Page * const page = new (new Byte [sizeof (Page) + page_size]) Page (0);
	    assert (page);

	    page->next_pool_page = first_spare_page;
	    first_spare_page = page;

	    ++stats.num_spare_pages;
	    ++num_pages;
	}
    }

    mutex.unlock ();
}

PagePool::PagePool (Size  const page_size,
		    Count const min_pages)
    : page_size (page_size),
      min_pages (min_pages),
      num_pages (min_pages),
      first_spare_page (NULL)
{
    Page *prv_page = NULL;
    for (Count i = 0; i < min_pages; ++i) {
	Page * const page = new (new Byte [sizeof (Page) + page_size]) Page (0 /* TODO: Set to 1 for testing (should be 0) */);
	assert (page);
	if (!first_spare_page)
	    first_spare_page = page;

	if (prv_page)
	    prv_page->next_pool_page = page;

	prv_page = page;
    }
    if (prv_page)
	prv_page->next_pool_page = NULL;

    stats.num_busy_pages = 0;
    stats.num_spare_pages = min_pages;
}

PagePool::~PagePool ()
{
    mutex.lock ();

    assert (!stats.num_busy_pages);

    Page *cur_page = first_spare_page;
    while (cur_page) {
	Page * const next_page = cur_page->next_pool_page;
	// FIXME mismatch free/delete/delete[] complaints from valgrind.
	cur_page->~Page();
	delete[] (Byte*) cur_page;
	cur_page = next_page;
    }

    mutex.unlock ();
}

}

