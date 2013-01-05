/** @file materialscheme.cpp Material system subspace scheme.
 *
 * @author Copyright &copy; 2010-2012 Daniel Swanson <danij@dengine.net>
 *
 * @par License
 * GPL: http://www.gnu.org/licenses/gpl.html
 *
 * <small>This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version. This program is distributed in the hope that it
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General
 * Public License for more details. You should have received a copy of the GNU
 * General Public License along with this program; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA</small>
 */

#include "resource/materialbind.h"
#include "resource/materialscheme.h"

namespace de {

struct MaterialScheme::Instance
{
    /// Symbolic name of the scheme.
    String name;

    /// Mappings from paths to manifests.
    MaterialScheme::Index *index;

    Instance(String symbolicName)
        : name(symbolicName), index(new MaterialScheme::Index())
    {}

    ~Instance()
    {
        if(index)
        {
            DENG_ASSERT(index->isEmpty());
            delete index;
        }
    }
};

MaterialScheme::MaterialScheme(String symbolicName)
{
    d = new Instance(symbolicName);
}

MaterialScheme::~MaterialScheme()
{
    clear();
    delete d;
}

void MaterialScheme::clear()
{
    if(d->index)
    {
        PathTreeIterator<Index> iter(d->index->leafNodes());
        while(iter.hasNext())
        {
            MaterialBind *mb = reinterpret_cast<MaterialBind *>(iter.next().userPointer());
            if(mb)
            {
                // Detach our user data from this node.
                iter.value().setUserPointer(0);
                delete mb;
            }
        }
        d->index->clear();
    }
}

String const &MaterialScheme::name() const
{
    return d->name;
}

int MaterialScheme::size() const
{
    return d->index->size();
}

MaterialScheme::Index::Node &MaterialScheme::insertNode(Path const &path)
{
    return d->index->insert(path);
}

MaterialScheme::Index::Node const &MaterialScheme::find(Path const &path) const
{
    try
    {
        return d->index->find(path, Index::NoBranch | Index::MatchFull);
    }
    catch(Index::NotFoundError const &er)
    {
        throw NotFoundError("MaterialScheme::find", er.asText());
    }
}

MaterialScheme::Index::Node &MaterialScheme::find(Path const &path)
{
    Index::Node const &found = const_cast<MaterialScheme const *>(this)->find(path);
    return const_cast<Index::Node &>(found);
}

MaterialScheme::Index const &MaterialScheme::index() const
{
    return *d->index;
}

} // namespace de
