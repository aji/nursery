# Roadmap

This document is subject to change as development progresses to 1.0. Child
entries are necessarily dependencies of the parent. An arrow (&rarr;) before a
name indicates a non-child dependency. In this way, the parent-child
relationship is merely an organizational one that reflects the "first"
dependency, or perhaps "most direct" dependency.

* [ ] **Renderer core** `render`
    * [ ] **Rendering equation solver**  
          &rarr; *BSDF framework*  
          &rarr; *Flexible scene hierarchy*
* [ ] **BSDF framework** `material`
* [ ] **Parallelization core** `distr`
    * [ ] **Master scheduler**
    * [ ] **Worker**
        * [ ] **CPU worker**
        * [ ] *GPGPU worker (nice to have)*  
              &rarr; *Pointer-free flattening*
    * [ ] **RPC framework**
    * [ ] *Web interface (nice to have)*
* [ ] **Flexible scene hierarchy** `scene`
    * [ ] **Volume groups**
    * [ ] *Pointer-free flattening (nice to have)*
* [ ] **Scene description** `lang`
    * [ ] **cray scene language** `csl`  
          &rarr; *Flexible scene hierarchy*
    * [ ] **cray material language** `cml`  
          &rarr; *BSDF framework*
    * [ ] *Export from Blender (nice to have)*
