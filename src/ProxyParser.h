//

// implements PROXY v2 protocol



struct ProxyParser {
    int done = false;

    // 16 byte IP, 2 byte port
    // 16 byte our IP, 2 byte our port

    // return true when done, always return next offset for http parsing
    std::pair<bool, unsigned int> parse(std::string_view data) {

        /* If already parsed, we're done */
        if (done) {
            return {true, 0};
        }

        // we require 4 bytes to determine if this is http or not
        if (data.length() < 4) {
            // we are not done, buffer everything
            return {false, 0};
        } else {


            // is this proxy protocol?
            if (memcmp("\r\n\r\n", data.data(), 4) == 0) {



            } else {
                // it cannot be proxy protocol here, so we are done now
                done = true;
                return {true, 0};
            }


        }

    }

};