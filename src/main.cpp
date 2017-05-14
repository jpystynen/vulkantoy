// Copyright (c) 2017 Johannes Pystynen
// This code is licensed under the MIT license (MIT)

#include "Engine.h"

#include <iostream>
#include <stdexcept>

///////////////////////////////////////////////////////////////////////////////

int main()
{
    try
    {
        core::Engine app;

        app.init();
        app.run();
    }
    catch (const std::runtime_error& err)
    {
        std::cerr << err.what() << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}

///////////////////////////////////////////////////////////////////////////////
