## 2024-05-19 - Regex Compilation Overhead
**Learning:** `FRegexPattern` compilation in tight loops or frequently called validation functions (like `HCI_IsVariableTemplateString` or `HCI_ParseVariableTemplate`) introduces measurable overhead when instantiated as local `const` variables, as the ICU regex engine recompiles the pattern on every invocation.
**Action:** Use `static const FRegexPattern Pattern(...)` for fixed string patterns inside functions to ensure the regex is compiled only once and reused across subsequent calls, leveraging C++11 thread-safe static initialization.
