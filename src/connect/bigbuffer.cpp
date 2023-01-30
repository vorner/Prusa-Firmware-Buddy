#include "bigbuffer.hpp"

#include <cstring>

namespace connect_client {

MarlinPaths::MarlinPaths() {
    memset(name.begin(), 0, name.size());
    memset(path.begin(), 0, path.size());
}

}
