/** @file asynctask.h  Asynchoronous task with a completion callback.
 *
 * @authors Copyright (c) 2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 *
 * @par License
 * LGPL: http://www.gnu.org/licenses/lgpl.html
 *
 * <small>This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or (at your
 * option) any later version. This program is distributed in the hope that it
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser
 * General Public License for more details. You should have received a copy of
 * the GNU Lesser General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small>
 */

#ifndef LIBDENG2_ASYNCTASK_H
#define LIBDENG2_ASYNCTASK_H

#include "../App"
#include "../Loop"
#include "../String"

#include <QThread>
#include <utility>

namespace de {

struct DENG2_PUBLIC AsyncTask : public QThread
{
    virtual ~AsyncTask() {}
    virtual void abort() = 0;
};

namespace internal {

template <typename Task, typename Completion>
class AsyncTaskThread : public AsyncTask
{
    Task task;
    Completion completion;
    decltype(task()) result {}; // can't be void

    void run() override
    {
        try
        {
            result = task();
        }
        catch (...)
        {}
        notifyCompletion();
    }

    void notifyCompletion()
    {
        Loop::mainCall([this] ()
        {
            completion(result);
            deleteLater();
        });
    }

public:
    AsyncTaskThread(Task const &task, Completion const &completion)
        : task(task)
        , completion(completion)
    {}

    void abort() override
    {
        terminate();
        notifyCompletion();
    }
};

} // namespace internal

/**
 * Executes an asynchronous callback in a background thread. After the background thread
 * finishes, the result from the callback is passed to another callback that is called
 * in the main thread.
 *
 * Must be called from the main thread.
 *
 * @param task        Task callback. If an exception is thrown here, it will be
 *                    quietly caught, and the completion callback will be called with
 *                    a default-constructed result value.
 * @param completion  Completion callback. Takes one argument matching the type of
 *                    the return value from @a task.
 *
 * @return Background thread object. The thread will delete itself after the completion
 * callback has been called.
 */
template <typename Task, typename Completion>
AsyncTask *async(Task const &task, Completion const &completion)
{
    DENG2_ASSERT_IN_MAIN_THREAD();
    auto *t = new internal::AsyncTaskThread<Task, Completion>(task, completion);
    t->start();
    // Note: The thread will delete itself when finished.
    return t;
}

} // namespace de

#endif // LIBDENG2_ASYNCTASK_H