# AI Assistant Rules

## All AI-generated code in this codebase MUST be marked. 

Every identifier an AI adds MUST be suffixed with `_AI` — functions,
variables, parameters, types, members, enums and enumerators, macros,
namespaces, template parameters, constants. If an AI rewrites an existing
function's body, that function counts as AI-added and is renamed with `_AI`.
AI assistants MUST NOT remove an existing `_AI` suffix.

Exception: when a name is fixed by an external contract and cannot change —
an override, interface method, operator, a constructor/destructor of a
pre-existing type, or a framework-required entry point/callback — do NOT
suffix it. Mark it with a trailing `// _AI` comment instead.

Example:
```cpp
enum class Color_AI { Red_AI };        // new type + enumerator

struct Config_AI {                      // new type
    Color_AI color_AI;                  // new member
};

void existingFunction(NewType_AI param_AI) {  // param added to existing fn
    int newLocal_AI = {};
}

void newFunction_AI() {}                // new function

struct Handler_AI : Base {
    void onEvent() override {}          // _AI  (name fixed by Base)
};
```