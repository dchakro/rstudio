Tricks used to make the `rstudio-server` [build 2021.09.2](https://github.com/rstudio/rstudio/releases/tag/v2021.09.2%2B382) successfully build on an iMac running MacOS Catalina.

# FIX C++

### C++ : cmath error:

```sh
/Library/Developer/CommandLineTools/usr/bin/../include/c++/v1/cmath:317:9: error: no member named 'signbit' in the global namespace
using ::signbit
```

Solution:

As root, edit `/Library/Developer/CommandLineTools/usr/include/c++/v1/cmath` and replace the line (#304) that says:

```c++
#include <math.h>
```

and add the full path of math.h (present in the SAME folder, but it isn’t found for some reason). The line should look like:

```c++
#include </Library/Developer/CommandLineTools/usr/include/c++/v1/math.h>
```


# FIX JAVA
### JAVA : InaccessibleObjectException

```sh
     [java] Caused by: java.lang.reflect.InaccessibleObjectException: Unable to make protected final java.lang.Class java.lang.ClassLoader.defineClass(java.lang.String,byte[],int,int,java.security.ProtectionDomain) throws java.lang.ClassFormatError accessible: module java.base does not "opens java.lang" to unnamed module @2eb231a6
...
     [java]    [ERROR] Errors in 'org/rstudio/studio/client/RStudioGinjector.java'
     [java]       [ERROR] Line 346: Failed to resolve 'org.rstudio.studio.client.RStudioGinjector' via deferred binding
```

This error is caused because the `gwt` code [doesn't build](https://github.com/rstudio/rstudio/issues/9463) on higher versions of Java. Here's how to fix it.



### Set up java:

```sh
# This command would list all the java versions on your machine
/usr/libexec/java_home -V

# This will list the default java in use
/usr/libexec/java_home

# This is how to use another version than the default
/usr/libexec/java_home -v "11"
```

Set the environment variable `JAVA_HOME` using this line:

```sh
export JAVA_HOME=$(/usr/libexec/java_home)
```

Set it in `/etc/bashrc`, `/etc/zshc` and `/etc/profile`. Also define it at the top in `./build/src/gwt/CMakeFiles/gwt_build.dir/build.make`.

Set `JAVA` in `CMakeGlobals.txt` as the following diff:
```
find_package(Java)
message(STATUS "JAVA_HOME: ${JAVA_HOME}")
```

Then Build `gwt` using `ant` using:

```sh
# cd /Users/deepankar/rstudio-server/rstudio/src/gwt && /usr/local/bin/ant -Dbuild.dir="bin" -Dwww.dir="www" -Dextras.dir="extras" -Dlib.dir="lib" -Dgwt.main.module="org.rstudio.studio.RStudio"

sudo -s
cd /Users/deepankar/rstudio-server/rstudio/src/gwt
./ant -Dbuild.dir="bin" -Dwww.dir="www" -Dextras.dir="extras" -Dlib.dir="lib" -Dgwt.main.module="org.rstudio.studio.RStudio"
```

# Fix YAML parser

### Download and build yaml parser:

Rstudio had the code set up but didn’t have a make target for `libyaml-cpp.a`, here’s how to fix it.

```sh
# Download and compile the parser
curl -OJL https://github.com/jbeder/yaml-cpp/archive/refs/tags/yaml-cpp-0.6.3.tar.gz
tar -xvf yaml-cpp-yaml-cpp-0.6.3.tar.gz && cd yaml-cpp-yaml-cpp-0.6.3
make 
make install

# Link the library where rstudio expects it
cd /opt/rstudio-tools/x86_64/yaml-cpp/0.6.3/build/
sudo ln -s '/usr/local/lib/libyaml-cpp.a' ./libyaml-cpp.a

```

# INSTALL

Finally become root and then run just `make install`, for some reason running `sudo make install` as either a normal nor a privilaged user seems to work.

## Just Compile and Run

```sh
# Steps in short to get running quick & dirty

cd ~/rstudio-server/rstudio/dependencies/osx
./install-dependencies-osx
cd ~/rstudio-server/rstudio/src/cpp
mkdir build_version
cd build_version
cmake .. -DRSTUDIO_TARGET=Server -GNinja
ninja
cd ~/rstudio-server/rstudio/src/gwt
./ant
cd ~/rstudio-server/rstudio/src/cpp/build
./rserver-dev
```

This method allows to build easily without the troubles of `make install`. Less headache.

# CONFIG

### Create a user for rstudio-server

```sh
dscl . -create /Users/rstudio-server
dscl . -create /Users/rstudio-server UserShell /bin/zsh
dscl . -create /Users/rstudio-server RealName "Rstudio Server" 
dscl . -create /Users/rstudio-server UniqueID "510"
dscl . -create /Users/rstudio-server PrimaryGroupID 20
dscl . -create /Users/rstudio-server NFSHomeDirectory /Users/rstudio-server
dscl . -passwd /Users/rstudio-server runrstudio 
dscl . create /Users/rstudio-server IsHidden 1
sudo reboot now
```
