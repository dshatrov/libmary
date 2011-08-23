#ifndef __LIBMARY__VSTACK__H__
#define __LIBMARY__VSTACK__H__


#include <libmary/types.h>
#include <libmary/intrusive_list.h>


namespace M {

class VStack
{
public:
    typedef Size Level;

private:
    class Block : public IntrusiveListElement<>
    {
    public:
	Byte *buf;

	Size start_level;
	Size height;
    };

    typedef IntrusiveList<Block> BlockList;

    Size const block_size;
    bool const shrinking;

    Size level;

    BlockList block_list;
    Block *cur_block;

    Byte* addBlock (Size const num_bytes)
    {
	Byte *ret_buf = NULL;

	if (cur_block != NULL &&
	    block_list.getNext (cur_block) != NULL)
	{
	  // Reusing allocated block.
	    Block * const block = block_list.getNext (cur_block);
	    block->start_level = level;
	    block->height = num_bytes;

	    cur_block = block;
	    ret_buf = block->buf;
	} else {
	  // Allocating a new block.
	    // TODO 'block.buf' could be allocated along with 'block'
	    // in the same chunk of memory.
	    Block * const block = new Block;
	    block_list.append (block);

	    block->buf = new Byte [block_size];
	    block->start_level = level;
	    block->height = num_bytes;

	    cur_block = block_list.getLast();
	    ret_buf = block->buf;
	}

	level += num_bytes;

	return ret_buf;
    }

public:
    Byte* push (Size num_bytes)
    {
	assert (num_bytes <= block_size);

	if (block_list.isEmpty () ||
	    block_size - cur_block->height < num_bytes)
	{
	    return addBlock (num_bytes);
	}

	Block * const block = cur_block;

	Size const prv_height = block->height;
	block->height += num_bytes;

	level += num_bytes;

	return block->buf + prv_height;
    }

    Byte* push_malign (Size num_bytes)
    {
	return push_malign (num_bytes, num_bytes);
    }

    // Returned address meets alignment requirements for an object of class A
    // if sizeof(A) is @alignment.
    // Returned address is always a multiple of @alignment bytes away from
    // the start of the corresponding block.
    Byte* push_malign (Size num_bytes, Size alignment)
    {
	assert (num_bytes <= block_size);

	if (block_list.isEmpty () ||
	    block_size - cur_block->height < num_bytes)
	{
	    return addBlock (num_bytes);
	}

	Block * const block = cur_block;

	Size new_height = block->height + num_bytes;
	Size delta = new_height % alignment;
	if (delta > 0)
	    new_height = new_height - delta + alignment;

	if (new_height > block_size) {
	    return addBlock (num_bytes);
	}

	Size const prv_height = block->height;
	block->height = new_height;

	level += new_height - prv_height;

	return block->buf + prv_height;
    }

    Level getLevel () const
    {
	return level;
    }

    void setLevel (Level const &new_level)
    {
	if (!block_list.isEmpty ()) {
	    if (cur_block->start_level > new_level) {
		Block * const prv_block = block_list.getPrevious (cur_block);

		if (shrinking) {
		  // Deleting block.
		    delete[] cur_block->buf;
		    block_list.remove (cur_block);
		    delete cur_block;
		  // 'cur_block' is not valid anymore
		}

		cur_block = prv_block;
	    } else {
		cur_block->height = new_level - cur_block->start_level;
	    }
	}

	level = new_level;
    }

    VStack (Size block_size /* > 0 */,
	    bool shrinking = false)
	: block_size (block_size),
	  shrinking (shrinking),
	  level (0),
	  cur_block (NULL)
    {
    }

    ~VStack ()
    {
	BlockList::iter iter (block_list);
	while (!block_list.iter_done (iter)) {
	    Block * const block = block_list.iter_next (iter);
	    delete[] block->buf;
	    delete block;
	}
    }
};

}


#endif /* __LIBMARY__VSTACK__H__ */

