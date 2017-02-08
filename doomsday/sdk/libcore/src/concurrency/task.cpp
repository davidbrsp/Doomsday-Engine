/** @file task.cpp  Concurrent task.
 *
 * @authors Copyright © 2013-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "de/Task"
#include "de/TaskPool"
#include "de/Log"

namespace de {

Task::Task() : _pool(nullptr)
{}

void Task::run()
{
    try
    {
        runTask();
    }
    catch (Error const &er)
    {
        LOG_AS("Task");
        LOG_WARNING("Aborted due to exception: ") << er.asText();
    }

    // Cleanup.
    if (_pool) _pool->taskFinishedRunning(*this);
    
    // The thread's log is not disposed because task threads are pooled (by TaskPool)
    // and the log object will be reused in future tasks.
}

} // namespace de
