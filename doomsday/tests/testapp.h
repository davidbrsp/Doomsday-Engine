/*
 * The Doomsday Engine Project
 *
 * Copyright (c) 2009 Jaakko Keränen <jaakko.keranen@iki.fi>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#ifndef TESTAPP_H
#define TESTAPP_H

#include <de/App>

class TestApp : public de::App
{
public:
    TestApp(const de::CommandLine& args) : de::App(args, "/config/testapp.de") {
        std::cout << "TestApp constructed.\n";
    }
    
    ~TestApp() {
        std::cout << "TestApp destructed.\n";
    }

    void iterate() {}
};

#endif /* TESTAPP_H */
