Tricks used to make the `rstudio-server` [build 1.4.1717](https://github.com/rstudio/rstudio/releases/tag/v1.4.1717) successfully build on MacOS catalina.



### Get JDK8 using brew:

```sh
brew tap AdoptOpenJDK/openjdk
brew install --cask adoptopenjdk/openjdk/adoptopenjdk8

# Verify
/usr/libexec/java_home -V
```

### Set up java:

```sh
# This command would list all the java versions on your machine
/usr/libexec/java_home -V

# This will list the default java in use
/usr/libexec/java_home

# This is how to use another version than the default
/usr/libexec/java_home -v "1.8"
```

Then disable all the other JDKs using this [neat trick](https://stackoverflow.com/a/44169445) : basically go to `/Library/Java/JavaVirtualMachines` and for every JDK, go into the directrory `./Contents`and  run `sudo mv Info.plist Info.plist.inactive`. This disables the system accidentally fetching one of the unwanted `java` versions (generally, it selects the latest `java`). 

You can simply rename the `plist` and it would be recognized by the system. This makes it easy as you don’t have to completely uninstall the various JDKs.

Set the environment variable `JAVA_HOME` using this line:

```sh
export JAVA_HOME=$(/usr/libexec/java_home)
```

Set it in `/etc/bashrc`, `/etc/zshc` and `/etc/profile`. Also define it at the top in `src/gwt/CMakeFiles/gwt_build.dir/build.make`.



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



### JAVA : InaccessibleObjectException

```sh
     [java] Caused by: java.lang.reflect.InaccessibleObjectException: Unable to make protected final java.lang.Class java.lang.ClassLoader.defineClass(java.lang.String,byte[],int,int,java.security.ProtectionDomain) throws java.lang.ClassFormatError accessible: module java.base does not "opens java.lang" to unnamed module @2eb231a6
...
     [java]    [ERROR] Errors in 'org/rstudio/studio/client/RStudioGinjector.java'
     [java]       [ERROR] Line 346: Failed to resolve 'org.rstudio.studio.client.RStudioGinjector' via deferred binding
```



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



### Correctly run the installation

Finally become root and then run just `make install`, for some reason running `sudo make install` as either a normal nor a privilaged user seems to work.



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

