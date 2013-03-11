/*
This file is part of Bohrium and Copyright (c) 2012 the Bohrium team:
http://bohrium.bitbucket.org

Bohrium is free software: you can redistribute it and/or modify
it under the terms of the GNU Lesser General Public License as 
published by the Free Software Foundation, either version 3 
of the License, or (at your option) any later version.

Bohrium is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the 
GNU Lesser General Public License along with bohrium. 

If not, see <http://www.gnu.org/licenses/>.
*/
#include <iostream>
#include "bh/cppb.hpp"

using namespace bh;

void compute()
{
    multi_array<double> x(3,3);
    multi_array<double> y(3,3);
    multi_array<double> z(3,3);
    
    //x = 1.0;
    y = 1.0;
    z = 2.0;

    // vcache issue!
    for(int i=0; i<20000; i++) {

        x = y + z;
        std::cout   << i << ": [x=" << x.getKey()  \
                    << ",y=" << y.getKey()    \
                    << ",z=" << z.getKey()  \
                    << "] [" << storage.size() << "] ";
        pprint( x );
    }

}

int main()
{
    std::cout << "LOOP example." << std::endl;

    compute();

    return 0;
}

