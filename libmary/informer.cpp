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


#include <libmary/informer.h>


namespace M {

// Informer is a helper object for signaling asynchronous events in MT-safe
// manner.

void
GenericInformer::releaseSubscription (Subscription * const mt_nonnull sbn)
{
    if (sbn->weak_code_ref.isValid()) {
	CodeRef const code_ref = sbn->weak_code_ref;
	if (code_ref)
	    code_ref->removeDeletionCallback_mutualUnlocked (sbn->del_sbn);
    }

    sbn->ref_data = NULL;
}

void
GenericInformer::subscriberDeletionCallback (void *_sbn)
{
    Subscription * const sbn = static_cast <Subscription*> (_sbn);
    GenericInformer * const self = sbn->informer;

    self->mutex->lock ();
    self->sbn_list.remove (sbn);
    self->mutex->unlock ();

    delete sbn;
}

void
GenericInformer::informAll (ProxyInformCallback   const mt_nonnull proxy_inform_cb,
			    VoidFunction          const inform_cb,
			    void                * const inform_cb_data)
{
    mutex->lock ();
    informAll_unlocked (proxy_inform_cb, inform_cb, inform_cb_data);
    mutex->unlock ();
}

void
GenericInformer::informAll_unlocked (ProxyInformCallback   const mt_nonnull proxy_inform_cb,
				     VoidFunction          const inform_cb,
				     void                * const inform_cb_data)
{
    ++traversing;

    Subscription *sbn = sbn_list.getFirst();
    while (sbn) {
	if (!sbn->valid) {
	    sbn = sbn_list.getNext (sbn);
	    continue;
	}

	if (sbn->oneshot)
	    sbn->valid = false;

	CodeRef code_ref;
	if (sbn->weak_code_ref.isValid()) {
	    LibMary_ThreadLocal * const tlocal = libMary_getThreadLocal();
	    if (sbn->weak_code_ref.getWeakObject() != tlocal->last_coderef_container) {
		code_ref  = sbn->weak_code_ref;
		if (code_ref) {
		    mutex->unlock ();

		    Object * const prv_coderef_container = tlocal->last_coderef_container;
		    tlocal->last_coderef_container = sbn->weak_code_ref.getWeakObject();

		    proxy_inform_cb (sbn->cb_ptr, sbn->cb_data, inform_cb, inform_cb_data);

		    tlocal->last_coderef_container = prv_coderef_container;

		    mutex->lock ();
		    sbn = sbn_list.getNext (sbn);
		    continue;
		} else {
		    sbn = sbn_list.getNext (sbn);
		    continue;
		}
	    }
	}

	mutex->unlock ();

	proxy_inform_cb (sbn->cb_ptr, sbn->cb_data, inform_cb, inform_cb_data);

	mutex->lock ();
	sbn = sbn_list.getNext (sbn);
    }

    --traversing;
    if (traversing == 0) {
#if 0
// Old variant.
	sbn = sbn_list.getFirst();
	while (sbn) {
	    Subscription * const next_sbn = sbn_list.getNext (sbn);

	    if (!sbn->valid) {
		releaseSubscription (sbn);
		sbn_list.remove (sbn);
		delete sbn;
	    }

	    sbn = next_sbn;
	}
#endif
	sbn = sbn_invalidation_list.getFirst();
	while (sbn) {
	    Subscription * const next_sbn = sbn_invalidation_list.getNext (sbn);
	    assert (!sbn->valid);

	    releaseSubscription (sbn);
	    sbn_list.remove (sbn);
	    delete sbn;

	    sbn = next_sbn;
	}
	sbn_invalidation_list.clear ();
    }
}

GenericInformer::SubscriptionKey
GenericInformer::subscribeVoid (CallbackPtr      const cb_ptr,
				void           * const cb_data,
				VirtReferenced * const ref_data,
				Object         * const coderef_container)
{
    Subscription * const sbn = new Subscription (cb_ptr, cb_data, ref_data, coderef_container);
    sbn->valid = true;
    sbn->informer = this;
    sbn->oneshot = false;

    if (coderef_container) {
	// TODO It would be more effective lock 'mutex' earlier and call
	// addDeletionCallback_mutualUnlocked() here.
	sbn->del_sbn = coderef_container->addDeletionCallback (
		subscriberDeletionCallback, sbn, NULL /* ref_data */, getCoderefContainer());
    } else {
	sbn->del_sbn = NULL;
    }

    mutex->lock ();
    sbn_list.prepend (sbn);
    mutex->unlock ();

    return sbn;
}

GenericInformer::SubscriptionKey
GenericInformer::subscribeVoid_unlocked (CallbackPtr      const cb_ptr,
					 void           * const cb_data,
					 VirtReferenced * const ref_data,
					 Object         * const coderef_container)
{
    Subscription * const sbn = new Subscription (cb_ptr, cb_data, ref_data, coderef_container);
    sbn->valid = true;
    sbn->informer = this;
    sbn->oneshot = false;

    if (coderef_container) {
	// 'mutex' always points to 'getCoderefContainer()->mutex'. This means
	// that we must use addDeletionCallback_mutualUnlocked() here.
	sbn->del_sbn = coderef_container->addDeletionCallback_mutualUnlocked (
		subscriberDeletionCallback, sbn, NULL /* ref_data */, getCoderefContainer());
    } else {
	sbn->del_sbn = NULL;
    }

    sbn_list.prepend (sbn);

    return sbn;
}

void
GenericInformer::unsubscribe (SubscriptionKey const sbn_key)
{
    mutex->lock ();
    sbn_key.sbn->valid = false;
    if (traversing == 0) {
	releaseSubscription (sbn_key.sbn);
	sbn_list.remove (sbn_key.sbn);
	mutex->unlock ();
	delete sbn_key.sbn;
	return;
    }
    sbn_invalidation_list.append (sbn_key.sbn);
    mutex->unlock ();
}

void
GenericInformer::unsubscribe_unlocked (SubscriptionKey const sbn_key)
{
    sbn_key.sbn->valid = false;
    if (traversing == 0) {
	releaseSubscription (sbn_key.sbn);
	sbn_list.remove (sbn_key.sbn);
	delete sbn_key.sbn;
    }
    sbn_invalidation_list.append (sbn_key.sbn);
}

GenericInformer::~GenericInformer ()
{
    mutex->lock ();

    assert (sbn_invalidation_list.isEmpty());

    Subscription *sbn = sbn_list.getFirst();
    while (sbn) {
	Subscription * const next_sbn = sbn_list.getNext (sbn);

	releaseSubscription (sbn);
	delete sbn;

	sbn = next_sbn;
    }

    mutex->unlock ();
}

}

