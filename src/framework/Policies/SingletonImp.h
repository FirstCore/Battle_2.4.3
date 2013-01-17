/*
 * Copyright (C) 2011-2013 BlizzLikeCore <http://blizzlike.servegame.com/>
 * Please, read the credits file.
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef BLIZZLIKE_SINGLETONIMPL_H
#define BLIZZLIKE_SINGLETONIMPL_H

#include "Singleton.h"

// avoid the using namespace here cuz
// its a .h file afterall

template
<
typename T,
class ThreadingModel,
class CreatePolicy,
class LifeTimePolicy
>
T&
BlizzLike::Singleton<T, ThreadingModel, CreatePolicy, LifeTimePolicy >::Instance()
{
    if (!si_instance)
    {
        // double-checked Locking pattern
        Guard();
        if (!si_instance)
        {
            if (si_destroyed)
            {
                si_destroyed = false;
                LifeTimePolicy::OnDeadReference();
            }
            si_instance = CreatePolicy::Create();
            LifeTimePolicy::ScheduleCall(&DestroySingleton);
        }
    }

    return *si_instance;
}

template
<
typename T,
class ThreadingModel,
class CreatePolicy,
class LifeTimePolicy
>
void
BlizzLike::Singleton<T, ThreadingModel, CreatePolicy, LifeTimePolicy>::DestroySingleton()
{
    CreatePolicy::Destroy(si_instance);
    si_instance = NULL;
    si_destroyed = true;
}

#define INSTANTIATE_SINGLETON_1(TYPE) \
    template class BlizzLike::Singleton<TYPE, BlizzLike::SingleThreaded<TYPE>, BlizzLike::OperatorNew<TYPE>, BlizzLike::ObjectLifeTime<TYPE> >; \
    template<> TYPE* BlizzLike::Singleton<TYPE, BlizzLike::SingleThreaded<TYPE>, BlizzLike::OperatorNew<TYPE>, BlizzLike::ObjectLifeTime<TYPE> >::si_instance = 0; \
    template<> bool BlizzLike::Singleton<TYPE, BlizzLike::SingleThreaded<TYPE>, BlizzLike::OperatorNew<TYPE>, BlizzLike::ObjectLifeTime<TYPE> >::si_destroyed = false

#define INSTANTIATE_SINGLETON_2(TYPE, THREADINGMODEL) \
    template class BlizzLike::Singleton<TYPE, THREADINGMODEL, BlizzLike::OperatorNew<TYPE>, BlizzLike::ObjectLifeTime<TYPE> >; \
    template<> TYPE* BlizzLike::Singleton<TYPE, THREADINGMODEL, BlizzLike::OperatorNew<TYPE>, BlizzLike::ObjectLifeTime<TYPE> >::si_instance = 0; \
    template<> bool BlizzLike::Singleton<TYPE, THREADINGMODEL, BlizzLike::OperatorNew<TYPE>, BlizzLike::ObjectLifeTime<TYPE> >::si_destroyed = false

#define INSTANTIATE_SINGLETON_3(TYPE, THREADINGMODEL, CREATIONPOLICY ) \
    template class BlizzLike::Singleton<TYPE, THREADINGMODEL, CREATIONPOLICY, BlizzLike::ObjectLifeTime<TYPE> >; \
    template<> TYPE* BlizzLike::Singleton<TYPE, THREADINGMODEL, CREATIONPOLICY, BlizzLike::ObjectLifeTime<TYPE> >::si_instance = 0; \
    template<> bool BlizzLike::Singleton<TYPE, THREADINGMODEL, CREATIONPOLICY, BlizzLike::ObjectLifeType<TYPE> >::si_destroyed = false

#define INSTANTIATE_SINGLETON_4(TYPE, THREADINGMODEL, CREATIONPOLICY, OBJECTLIFETIME) \
    template class BlizzLike::Singleton<TYPE, THREADINGMODEL, CREATIONPOLICY, OBJECTLIFETIME >; \
    template<> TYPE* BlizzLike::Singleton<TYPE, THREADINGMODEL, CREATIONPOLICY, OBJECTLIFETIME >::si_instance = 0; \
    template<> bool BlizzLike::Singleton<TYPE, THREADINGMODEL, CREATIONPOLICY, OBJECTLIFETIME >::si_destroyed = false
#endif

