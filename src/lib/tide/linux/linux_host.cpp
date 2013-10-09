/**
* This file has been modified from its orginal sources.
*
* Copyright (c) 2012 Software in the Public Interest Inc (SPI)
* Copyright (c) 2012 David Pratt
* Copyright (c) 2012 Mital Vora
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
*   http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*
***
* Copyright (c) 2008-2012 Appcelerator Inc.
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
*   http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
**/

#include <tideutils/file_utils.h>
#include <tideutils/environment_utils.h>
using namespace TideUtils;

#include "../tide.h"

#include <dlfcn.h>
#include <gcrypt.h>
#include <gdk/gdk.h>
#include <gnutls/gnutls.h>
#include <gtk/gtk.h>
#include <pthread.h>

GCRY_THREAD_OPTION_PTHREAD_IMPL;
using Poco::ScopedLock;
using Poco::Mutex;

namespace tide
{
    static pthread_t mainThread = 0;

    static gboolean MainThreadJobCallback (gpointer data)
    {
        static_cast<Host*> (data)->RunMainThreadJobs();
        return TRUE;
    }

    void Host::Initialize (int argc, const char *argv[])
    {
        gtk_init (&argc, (char***) &argv);

        #ifdef USE_PRE_2_32_GLIB
        if (!g_thread_supported())
            g_thread_init (NULL);
        #endif

        mainThread = pthread_self();

        // Initialize gnutls for multi-threaded usage.
        gcry_control (GCRYCTL_SET_THREAD_CBS, &gcry_threads_pthread);
        gnutls_global_init();
    }

    Host::~Host()
    {
    }

    void Host::WaitForDebugger()
    {
        printf ("Waiting for debugger (Press Any Key to Continue pid=%i)...\n", getpid());
        getchar();
    }

    bool Host::RunLoop()
    {
        string origPath (EnvironmentUtils::Get ("KR_ORIG_LD_LIBRARY_PATH"));
        EnvironmentUtils::Set ("LD_LIBRARY_PATH", origPath);

        g_timeout_add (250, &MainThreadJobCallback, this);
        gtk_main();
        return false;
    }

    void Host::SignalNewMainThreadJob()
    {
    }

    void Host::ExitImpl (int exitCode)
    {
        // Only call this if gtk_main is running. If called when the gtk_main
        // is not running, it will cause an assertion failure.
        static bool mainLoopRunning = true;
        if (mainLoopRunning)
        {
            mainLoopRunning = false;
            gtk_main_quit();
        }
    }


    Module* Host::CreateModule (std::string& path)
    {
        void* handle = dlopen (path.c_str(), RTLD_LAZY | RTLD_GLOBAL);
        if (!handle)
        {
            throw ValueException::FromFormat ("Error loading module (%s): %s\n", path.c_str(), dlerror());
            return 0;
        }

        // get the module factory
        ModuleCreator* create = (ModuleCreator*) dlsym (handle, "CreateModule");
        if (!create)
        {
            throw ValueException::FromFormat (
                "Cannot load CreateModule symbol from module (%s): %s\n",
                path.c_str(), dlerror());
        }

        return create (this, FileUtils::GetDirectory (path).c_str());
    }

    bool Host::IsMainThread()
    {
        return pthread_equal (mainThread, pthread_self());
    }
}






