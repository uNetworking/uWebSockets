#include "../src/HttpRouter.h"

#include <cassert>
#include <iostream>

void testMethodPriority() {
    std::cout << "TestMethodPriority" << std::endl;
    uWS::HttpRouter<int> r;
    std::string result;

    r.add(r.methods, "/static/route", [&result](auto *) {
        std::cout << "ANY static route" << std::endl;
        result += "AS";
        return true;
    }, r.LOW_PRIORITY);

    r.add({"patch"}, "/static/route", [&result](auto *) {
        std::cout << "PATCH static route" << std::endl;
        result += "PS";
        return false;
    });

    r.add({"get"}, "/static/route", [&result](auto *) {
        std::cout << "GET static route" << std::endl;
        result += "GS";
        return true;
    });

    assert(r.route("nonsense", "/static/route") == false);
    assert(r.route("get", "/static") == false);
    assert(result == "");

    /* Should end up directly in ANY handler */
    result.clear();
    assert(r.route("post", "/static/route"));
    assert(result == "AS");

    /* Should up directly in GET handler */
    result.clear();
    assert(r.route("get", "/static/route"));
    assert(result == "GS");

    /* Should end up in PATCH then in ANY handler */
    result.clear();
    assert(r.route("patch", "/static/route"));
    assert(result == "PSAS");
}

void testPatternPriority() {
    std::cout << "TestPatternPriority" << std::endl;
    uWS::HttpRouter<int> r;
    std::string result;

    r.add(r.methods, "/a/b/c", [&result](auto *) {
        std::cout << "ANY static route" << std::endl;
        result += "AS";
        return false;
    }, r.LOW_PRIORITY);

    r.add({"get"}, "/a/:b/c", [&result](auto *) {
        std::cout << "GET parameter route" << std::endl;
        result += "GP";
        return false;
    });

    r.add({"get"}, "/a/*", [&result](auto *) {
        std::cout << "GET wildcard route" << std::endl;
        result += "GW";
        return false;
    });

    r.add({"get"}, "/a/b/c", [&result](auto *) {
        std::cout << "GET static route" << std::endl;
        result += "GS";
        return false;
    });

    r.add({"post"}, "/a/:b/c", [&result](auto *) {
        std::cout << "POST parameter route" << std::endl;
        result += "PP";
        return false;
    });

    r.add(r.methods, "/a/:b/c", [&result](auto *) {
        std::cout << "ANY parameter route" << std::endl;
        result += "AP";
        return false;
    }, r.LOW_PRIORITY);

    assert(r.route("post", "/a/b/c") == false);
    assert(result == "ASPPAP");

    result.clear();
    assert(r.route("get", "/a/b/c") == false);
    assert(result == "GSASGPAPGW");
}

void testUpgrade() {
    std::cout << "TestUpgrade" << std::endl;
    uWS::HttpRouter<int> r;
    std::string result;

    /* HTTP on / */
    r.add({"get"}, "/something", [&result](auto *) {
        result += "GS";
        return true;
    }, r.MEDIUM_PRIORITY);

    /* HTTP on /* */
    r.add({"get"}, "/*", [&result](auto *) {
        result += "GW";
        return false;
    }, r.MEDIUM_PRIORITY);

    /* WebSockets on /* */
    r.add({"get"}, "/*", [&result](auto *) {
        result += "WW";
        return false;
    }, r.HIGH_PRIORITY);

    assert(r.route("get", "/something"));
    assert(result == "WWGS");
    result.clear();

    assert(r.route("get", "/") == false);
    assert(result == "WWGW");
}

void testBugReports() {
    std::cout << "TestBugReports" << std::endl;
    {
        uWS::HttpRouter<int> r;
        std::string result;

        r.add({"get"}, "/foo//////bar/baz/qux", [&result](auto *) {
            result += "MANYSLASH";
            return false;
        }, r.MEDIUM_PRIORITY);

        r.add({"get"}, "/foo", [&result](auto *) {
            result += "FOO";
            return false;
        }, r.MEDIUM_PRIORITY);

        r.route("get", "/foo");
        r.route("get", "/foo/");
        r.route("get", "/foo//bar/baz/qux");
        r.route("get", "/foo//////bar/baz/qux");
        assert(result == "FOOMANYSLASH");
    }

    {
        uWS::HttpRouter<int> r;
        std::string result;

        r.add({"get"}, "/test/*", [&result](auto *) {
            result += "TEST";
            return false;
        }, r.MEDIUM_PRIORITY);

        r.route("get", "/test/");
        assert(result == "TEST");
    }

    {
        uWS::HttpRouter<int> r;
        std::string result;

        /* WS on /* */
        r.add({"get"}, "/*", [&result](auto *) {
            result += "WW";
            return false;
        }, r.HIGH_PRIORITY);

        /* HTTP on /ok */
        r.add({"get"}, "/ok", [&result](auto *) {
            result += "GS";
            return false;
        }, r.MEDIUM_PRIORITY);

        r.add({"get"}, "/*", [&result](auto *) {
            result += "GW";
            return false;
        }, r.MEDIUM_PRIORITY);

        r.route("get", "/ok");
        assert(result == "WWGSGW");
    }

    {
        uWS::HttpRouter<int> r;
        std::string result;

        /* WS on / */
        r.add({"get"}, "/", [&result](auto *) {
            result += "WS";
            return false;
        }, r.HIGH_PRIORITY);

        /* HTTP on / */
        r.add({"get"}, "/", [&result](auto *) {
            result += "GS";
            return false;
        }, r.MEDIUM_PRIORITY);

        r.route("get", "/");
        assert(result == "WSGS");
    }

    {
        uWS::HttpRouter<int> r;
        std::string result;

        /* WS on /* */
        r.add({"get"}, "/*", [&result](auto *) {
            result += "WW";
            return false;
        }, r.HIGH_PRIORITY);

        /* GET on /static */
        r.add({"get"}, "/static", [&result](auto *) {
            result += "GSL";
            return false;
        }, r.MEDIUM_PRIORITY);

        /* ANY on /* */
        r.add(r.methods, "/*", [&result](auto *) {
            result += "AW";
            return false;
        }, r.LOW_PRIORITY);

        r.route("get", "/static");
        assert(result == "WWGSLAW");
    }

    {
        uWS::HttpRouter<int> r;
        std::string result;

        /* WS on /* */
        r.add({"get"}, "/*", [&result](auto *) {
            result += "WW";
            return false;
        }, r.HIGH_PRIORITY);

        /* GET on / */
        r.add({"get"}, "/", [&result](auto *) {
            result += "GSS";
            return false;
        }, r.MEDIUM_PRIORITY);

        /* GET on /static */
        r.add({"get"}, "/static", [&result](auto *) {
            result += "GSL";
            return false;
        }, r.MEDIUM_PRIORITY);

        /* ANY on /* */
        r.add(r.methods, "/*", [&result](auto *) {
            result += "AW";
            return false;
        }, r.LOW_PRIORITY);

        r.route("get", "/static");
        assert(result == "WWGSLAW");
    }
}

void testParameters() {
    std::cout << "TestParameters" << std::endl;
    uWS::HttpRouter<int> r;
    std::string result;

    r.add({"get"}, "/candy/:kind/*", [&result](auto *h) {
        auto [paramsTop, params] = h->getParameters();

        assert(paramsTop == 0);
        assert(params[0] == "lollipop");

        result += "GPW";
        return false;
    });

    r.add({"get"}, "/candy/lollipop/*", [&result](auto *h) {
        auto [paramsTop, params] = h->getParameters();

        assert(paramsTop == -1);

        result += "GLW";
        return false;
    });

    r.add({"get"}, "/candy/:kind/:action", [&result](auto *h) {
        auto [paramsTop, params] = h->getParameters();

        assert(paramsTop == 1);
        assert(params[0] == "lollipop");
        assert(params[1] == "eat");

        result += "GPP";
        return false;
    });

    r.add({"get"}, "/candy/lollipop/:action", [&result](auto *h) {
        auto [paramsTop, params] = h->getParameters();

        assert(params[0] == "eat");
        assert(paramsTop == 0);

        result += "GLP";
        return false;
    });

    r.add({"get"}, "/candy/lollipop/eat", [&result](auto *h) {
        auto [paramsTop, params] = h->getParameters();

        assert(paramsTop == -1);

        result += "GLS";
        return false;
    });

    r.route("get", "/candy/lollipop/eat");
    assert(result == "GLSGLPGLWGPPGPW");
    result.clear();

    r.route("get", "/candy/lollipop/");
    r.route("get", "/candy/lollipop");
    r.route("get", "/candy/");
    assert(result == "GLWGPW");
}

int main() {
    testPatternPriority();
    testMethodPriority();
    testUpgrade();
    testBugReports();
    testParameters();
}
