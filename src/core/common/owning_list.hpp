/*
 *  Copyright (c) 2021, The OpenThread Authors.
 *  All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions are met:
 *  1. Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *  3. Neither the name of the copyright holder nor the
 *     names of its contributors may be used to endorse or promote products
 *     derived from this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 *  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 *  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 *  ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 *  LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 *  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 *  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 *  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 *  CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 *  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *  POSSIBILITY OF SUCH DAMAGE.
 */

/**
 * @file
 *   This file includes definitions for a singly linked list which owns its entries and frees them upon destruction of
 *   the list.
 */

#ifndef OWNING_LIST_HPP_
#define OWNING_LIST_HPP_

#include "openthread-core-config.h"

#include "common/linked_list.hpp"
#include "common/owned_ptr.hpp"

namespace ot {

/**
 * Represents a singly linked list which owns its entries and frees them upon destruction of the
 * list.
 */
template <typename Type> class OwningList : public LinkedList<Type>
{
    class Iterator;
    class ConstIterator;

public:
    /**
     * This is the default constructor for `OwningList`
     */
    OwningList(void) = default;

    /**
     * This is the destructor for `OwningList`.
     *
     * On destruction, all existing entries in the list are freed.
     */
    ~OwningList(void) { Free(); }

    /**
     * Clears the list and frees all existing entries in it.
     */
    void Free(void)
    {
        while (!Pop().IsNull())
        {
        }
    }

    /**
     * Clears the list and frees all existing entries in it.
     */
    void Clear(void) { Free(); }

    /**
     * Pops an entry from head of the linked list and return an `OwnedPtr` to it.
     *
     * @note This method does not change the popped entry itself, i.e., the popped entry next pointer stays as before.
     *
     * @returns An `OwnedPtr` to the entry that was popped (set to null if list of empty).
     */
    OwnedPtr<Type> Pop(void) { return OwnedPtr<Type>(LinkedList<Type>::Pop()); }

    /**
     * Pops an entry after a given previous entry.
     *
     * @note This method does not change the popped entry itself, i.e., the popped entry next pointer stays as before.
     *
     * @param[in] aPrevEntry  A pointer to a previous entry. If it is not `nullptr` the entry after this will be popped,
     *                        otherwise (if it is `nullptr`) the entry at the head of the list is popped.
     *
     * @returns An `OwnedPtr` to the entry that was popped (set to null if there is no entry to pop).
     */
    OwnedPtr<Type> PopAfter(Type *aPrevEntry) { return OwnedPtr<Type>(LinkedList<Type>::PopAfter(aPrevEntry)); }

    /**
     * Removes an entry matching a given set of conditions from the linked list.
     *
     * To check that an entry matches, the `Matches()` method is invoked on each `Type` entry in the list. The
     * `Matches()` method with the same set of `Args` input types should be provided by the `Type` class accordingly:
     *
     *      bool Type::Matches(const Args &...) const
     *
     * @note This method does not change the removed entry itself (which is returned in case of success), i.e., the
     * entry next pointer stays as before.
     *
     * @param[in]  aArgs       The args to pass to `Matches()`.
     *
     * @returns An `OwnedPtr` to the entry that was removed (set to null if there is no matching entry to remove).
     */
    template <typename... Args> OwnedPtr<Type> RemoveMatching(const Args &...aArgs)
    {
        return OwnedPtr<Type>(LinkedList<Type>::RemoveMatching(aArgs...));
    }

    /**
     * Removes all entries in the list matching given set of conditions from the list and adds them to a new list.
     *
     * To check that an entry matches, the `Matches()` method is invoked on each `Type` entry in the list. The
     * `Matches()` method with the same set of `Args` input types should be provided by the `Type` class accordingly:
     *
     *      bool Type::Matches(const Args &...) const
     *
     * The ownership of the removed entries is transferred from the original list to the @p aRemovedList.
     *
     * @param[in] aRemovedList The list to add the removed entries to.
     * @param[in] aArgs       The args to pass to `Matches()`.
     */
    template <typename... Args> void RemoveAllMatching(OwningList &aRemovedList, const Args &...aArgs)
    {
        LinkedList<Type>::RemoveAllMatching(aRemovedList, aArgs...);
    }

    /**
     * Removes and frees all entries in the list matching a given set of conditions.
     *
     * To check that an entry matches, the `Matches()` method is invoked on each `Type` entry in the list. The
     * `Matches()` method with the same set of `Args` input types should be provided by the `Type` class accordingly:
     *
     *      bool Type::Matches(const Args &...) const
     *
     * @param[in] aArgs       The args to pass to `Matches()`.
     *
     * @retval TRUE    At least one matching entry was removed.
     * @retval FALSE   No matching entry was found.
     */
    template <typename... Args> bool RemoveAndFreeAllMatching(const Args &...aArgs)
    {
        OwningList removedList;

        RemoveAllMatching(removedList, aArgs...);
        return !removedList.IsEmpty();
    }
};

} // namespace ot

#endif // OWNING_LIST_HPP_
