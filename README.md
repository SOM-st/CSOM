WebAssembly CSOM
----------------

 - install [emscripten](http://kripken.github.io/emscripten-site/docs/getting_started/downloads.html)
 - load core `git submodule update --init`
 - build with: `emmake make clean && emmake make debug && emcc CSOM.bc -o CSOM.js -s MAIN_MODULE=1 --pre-js pre-test.js && emcc Smalltalk/SOMCore.bc -s SIDE_MODULE=1 -o Smalltalk/SOMCore.js`
 - run with: `node CSOM.js`

Known Issues
------------

 - not selectable in ostool, but hard coded WebAssembly target
 - SOM tests not all passing
 - only tested on macOS 10.12 Sierra

CSOM - The Simple Object Machine implemented in C
=================================================

Introduction
------------

SOM is a minimal Smalltalk dialect used to teach VM construction at the [Hasso
Plattner Institute][SOM]. It was originally built at the University of Århus
(Denmark) where it was also used for teaching.

Currently, implementations exist for Java (SOM), C (CSOM), C++ (SOM++), and
Squeak/Pharo Smalltalk (AweSOM).

A simple SOM Hello World looks like:

```Smalltalk
Hello = (
  run = (
    'Hello World!' println.
  )
)
```

This repository contains a plain C implementation of SOM, including an
implementation of the SOM standard library and a number of examples. Please see
the [main project page][SOMst] for links to the VM implementations.


CSOM can be built with Make:

    $ make

Afterwards, the tests can be executed with:

    ./som.sh -cp Smalltalk TestSuite/TestHarness.som
   
A simple Hello World program is executed with:

    ./som.sh -cp Smalltalk Examples/Hello.som

The debug version of CSOM can be built using the `debug` target:

    $ make debug

Information on previous authors are included in the AUTHORS file. This code is
distributed under the MIT License. Please see the LICENSE file for details.
Additional documentation, detailing for instance the object model and how to
implement primitives, is available in the `doc` folder.


Build Status
------------

Thanks to Travis CI, all commits of this repository are tested.
The current build status is: [![Build Status](https://travis-ci.org/SOM-st/CSOM.png?branch=master)](https://travis-ci.org/SOM-st/CSOM/)

 [SOM]: http://www.hpi.uni-potsdam.de/hirschfeld/projects/som/
 [SOMst]: https://travis-ci.org/SOM-st/


      
