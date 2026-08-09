/**
 * @file test-script-variable-types.cpp
 * @brief Focused parser, IR, and evaluator tests for scalar variable types.
 */
#include "pch.h"

static std::string CapturedVariableTypeOutput;

static VOID
CaptureVariableTypeOutput(CHAR * Message)
{
    if (Message)
        CapturedVariableTypeOutput.append(Message);
}

static BOOLEAN
RunVariableTypeScript(const CHAR * Script, const CHAR * ExpectedOutput)
{
    CapturedVariableTypeOutput.clear();
    hyperdbg_u_set_text_message_callback((PVOID)CaptureVariableTypeOutput);
    BOOLEAN Result = hyperdbg_u_test_script_engine((CHAR *)Script);
    hyperdbg_u_unset_text_message_callback();
    if (!Result || CapturedVariableTypeOutput != ExpectedOutput)
    {
        std::cerr << "Variable-type script failed: " << Script
                  << "\nExpected: " << ExpectedOutput
                  << "\nActual: " << CapturedVariableTypeOutput << std::endl;
        PSYMBOL_BUFFER Buffer = (PSYMBOL_BUFFER)ScriptEngineParse((CHAR *)Script);
        if (Buffer && !Buffer->Message)
        {
            for (UINT32 Index = 0; Index < Buffer->Pointer; Index++)
            {
                std::cerr << Index << ": type=" << Buffer->Head[Index].Type
                          << " len=" << Buffer->Head[Index].Len
                          << " value=" << Buffer->Head[Index].Value << std::endl;
            }
        }
        if (Buffer) RemoveSymbolBuffer(Buffer);
        return FALSE;
    }
    return TRUE;
}

static BOOLEAN
RunVariableTypeScriptExpectFailure(const CHAR * Script)
{
    CapturedVariableTypeOutput.clear();
    hyperdbg_u_set_text_message_callback((PVOID)CaptureVariableTypeOutput);
    BOOLEAN Result = hyperdbg_u_test_script_engine((CHAR *)Script);
    hyperdbg_u_unset_text_message_callback();
    if (Result)
    {
        std::cerr << "Variable-type script unexpectedly succeeded: " << Script << std::endl;
        return FALSE;
    }
    return TRUE;
}

static BOOLEAN
RunVariableTypeScriptExpectRuntimeFailure(const CHAR * Script)
{
    CapturedVariableTypeOutput.clear();
    hyperdbg_u_set_text_message_callback((PVOID)CaptureVariableTypeOutput);
    BOOLEAN Result = hyperdbg_u_test_script_engine((CHAR *)Script);
    hyperdbg_u_unset_text_message_callback();
    if (Result || CapturedVariableTypeOutput.find("ScriptEngineExecute") == std::string::npos)
    {
        std::cerr << "Expected a variable-type runtime failure: " << Script
                  << "\nActual: " << CapturedVariableTypeOutput << std::endl;
        return FALSE;
    }
    return TRUE;
}

static BOOLEAN
TestVariableTypeIrContract()
{
    if (sizeof(SYMBOL) != sizeof(UINT64) * 3 ||
        FUNC_TYPED_LOAD != 116 || FUNC_TYPED_STORE != 117 ||
        FUNC_AGGREGATE_COPY != 118 || FUNC_AGGREGATE_ZERO != 119 ||
        FUNC_CONVERT_FLOAT != 141 || FUNC_CAST_SCALAR != 142 ||
        FUNC_ADD_TYPED != 143 || FUNC_NEQ_TYPED != 158 ||
        FUNC_NEG_TYPED != 159 || FUNC_BITWISE_NOT_TYPED != 160 ||
        FUNC_LOGICAL_NOT_TYPED != 161 || FUNC_POINTER_DIFF != 162 ||
        SCRIPT_SCALAR_TYPE_INVALID != 0 || SCRIPT_SCALAR_TYPE_BOOL != 1 ||
        SCRIPT_SCALAR_TYPE_I8 != 2 || SCRIPT_SCALAR_TYPE_I16 != 3 ||
        SCRIPT_SCALAR_TYPE_I32 != 4 || SCRIPT_SCALAR_TYPE_I64 != 5 ||
        SCRIPT_SCALAR_TYPE_U8 != 6 || SCRIPT_SCALAR_TYPE_U16 != 7 ||
        SCRIPT_SCALAR_TYPE_U32 != 8 || SCRIPT_SCALAR_TYPE_U64 != 9 ||
        SCRIPT_SCALAR_TYPE_F32 != 10 || SCRIPT_SCALAR_TYPE_F64 != 11 ||
        SCRIPT_SCALAR_TYPE_POINTER != 12 || SCRIPT_SCALAR_TYPE_F80 != 13)
    {
        std::cerr << "Variable-type serialized IR constants changed unexpectedly" << std::endl;
        return FALSE;
    }

    CHAR Script[] = "{ unsigned char narrowValue = 0x12345; int value = narrowValue + 1; value %= 4; }";
    PSYMBOL_BUFFER Buffer = (PSYMBOL_BUFFER)ScriptEngineParse(Script);
    if (!Buffer || Buffer->Message)
    {
        std::cerr << "Variable-type IR script did not parse: "
                  << (Buffer && Buffer->Message ? Buffer->Message : "no buffer") << std::endl;
        if (Buffer) RemoveSymbolBuffer(Buffer);
        return FALSE;
    }

    BOOLEAN SawCast = FALSE;
    BOOLEAN SawAdd = FALSE;
    BOOLEAN SawModulo = FALSE;
    for (UINT32 Index = 0; Index < Buffer->Pointer; Index++)
    {
        if (Buffer->Head[Index].Type != SYMBOL_SEMANTIC_RULE_TYPE)
            continue;
        SawCast |= Buffer->Head[Index].Value == FUNC_CAST_SCALAR;
        SawAdd |= Buffer->Head[Index].Value == FUNC_ADD_TYPED;
        SawModulo |= Buffer->Head[Index].Value == FUNC_MOD_TYPED;
    }
    RemoveSymbolBuffer(Buffer);
    if (!SawCast || !SawAdd || !SawModulo)
    {
        std::cerr << "Variable-type IR is missing cast/add/modulo: "
                  << SawCast << "/" << SawAdd << "/" << SawModulo << std::endl;
    }
    return SawCast && SawAdd && SawModulo;
}

static BOOLEAN
TestStringVariableSemanticFile()
{
    CHAR Directory[MAX_PATH] = {0};
    if (!hyperdbg_u_setup_path_for_filename(SCRIPT_SEMANTIC_TEST_CASE_DIRECTORY,
                                            Directory,
                                            MAX_PATH,
                                            FALSE))
        return FALSE;

    std::filesystem::path FilePath = std::filesystem::path(Directory) / "string-variable-test-cases.ds";
    std::ifstream File(FilePath, std::ios::binary);
    if (!File)
    {
        std::cerr << "Could not open string semantic test file: " << FilePath.string() << std::endl;
        return FALSE;
    }
    std::string Script((std::istreambuf_iterator<char>(File)), std::istreambuf_iterator<char>());
    std::string::size_type CommandPrefix = Script.find('?');
    if (CommandPrefix != std::string::npos)
        Script.erase(CommandPrefix, 1);
    std::string Expected;
    for (UINT32 Case = 93; Case <= 104; Case++)
        Expected += "test case " + std::to_string(Case) + " was successful\n";
    return RunVariableTypeScript(Script.c_str(), Expected.c_str());
}

static BOOLEAN
TestVariableTypeSemanticCase88()
{
    CHAR Directory[MAX_PATH] = {0};
    if (!hyperdbg_u_setup_path_for_filename(SCRIPT_SEMANTIC_TEST_CASE_DIRECTORY,
                                            Directory,
                                            MAX_PATH,
                                            FALSE))
        return FALSE;

    std::filesystem::path FilePath = std::filesystem::path(Directory) / "variable-type-test-cases.ds";
    std::ifstream File(FilePath, std::ios::binary);
    if (!File)
        return FALSE;

    std::string Script((std::istreambuf_iterator<char>(File)), std::istreambuf_iterator<char>());
    std::string Marker = "// Test case 88 ";
    std::string::size_type MarkerPosition = Script.find(Marker);
    std::string::size_type CaseStart = Script.find("current_test_case = current_test_case + 1;", MarkerPosition);
    std::string::size_type CaseEnd = Script.find("/* ======================================================================= */", CaseStart);
    if (MarkerPosition == std::string::npos || CaseStart == std::string::npos || CaseEnd == std::string::npos)
        return FALSE;

    std::string Isolated = "{ test_case88 = 1; current_test_case = 0n87; " +
                           Script.substr(CaseStart, CaseEnd - CaseStart) + " }";
    return RunVariableTypeScript(Isolated.c_str(), "test case 88 was successful\n");
}

static BOOLEAN
TestManualSemanticCase56()
{
    CHAR Directory[MAX_PATH] = {0};
    if (!hyperdbg_u_setup_path_for_filename(SCRIPT_SEMANTIC_TEST_CASE_DIRECTORY,
                                            Directory,
                                            MAX_PATH,
                                            FALSE))
        return FALSE;

    std::filesystem::path FilePath = std::filesystem::path(Directory) / "manual-test-cases_50-59.ds";
    std::ifstream File(FilePath, std::ios::binary);
    if (!File)
        return FALSE;

    std::string Script((std::istreambuf_iterator<char>(File)), std::istreambuf_iterator<char>());
    std::string Marker = "// Test case 56 ";
    std::string::size_type MarkerPosition = Script.find(Marker);
    std::string::size_type CaseStart = Script.find("current_test_case = current_test_case + 1;", MarkerPosition);
    std::string::size_type CaseEnd = Script.find("/* ======================================================================= */", CaseStart);
    if (MarkerPosition == std::string::npos || CaseStart == std::string::npos || CaseEnd == std::string::npos)
        return FALSE;

    std::string Isolated = "{ test_case56 = 1; current_test_case = 0n55; " +
                           Script.substr(CaseStart, CaseEnd - CaseStart) + " }";
    return RunVariableTypeScript(Isolated.c_str(), "test case 56 was successful\n");
}

static BOOLEAN
TestManualSemanticCases40To49()
{
    CHAR Directory[MAX_PATH] = {0};
    if (!hyperdbg_u_setup_path_for_filename(SCRIPT_SEMANTIC_TEST_CASE_DIRECTORY,
                                            Directory,
                                            MAX_PATH,
                                            FALSE))
        return FALSE;

    std::filesystem::path FilePath = std::filesystem::path(Directory) / "manual-test-cases_40-49.ds";
    std::ifstream File(FilePath, std::ios::binary);
    if (!File)
        return FALSE;

    std::string Script((std::istreambuf_iterator<char>(File)), std::istreambuf_iterator<char>());
    std::string::size_type CommandPrefix = Script.find('?');
    if (CommandPrefix != std::string::npos)
        Script.erase(CommandPrefix, 1);

    std::string Expected;
    for (UINT32 Case = 40; Case <= 49; Case++)
    {
        std::string Marker = "// Test case " + std::to_string(Case) + " ";
        std::string::size_type MarkerPosition = Script.find(Marker);
        std::string::size_type CaseStart = Script.find("current_test_case = current_test_case + 1;", MarkerPosition);
        std::string::size_type CaseEnd = Script.find("/* ======================================================================= */", CaseStart);
        if (MarkerPosition == std::string::npos || CaseStart == std::string::npos)
            return FALSE;
        if (CaseEnd == std::string::npos)
            CaseEnd = Script.rfind('}');

        std::string Isolated = "{ test_case" + std::to_string(Case) + " = 1; current_test_case = 0n" +
                               std::to_string(Case - 1) + "; .variable_equal_to_55 = 0x55; " +
                               Script.substr(CaseStart, CaseEnd - CaseStart) + " }";
        std::string IsolatedExpected = "test case " + std::to_string(Case) + " was successful\n";
        if (!RunVariableTypeScript(Isolated.c_str(), IsolatedExpected.c_str()))
            return FALSE;
        Expected += "test case " + std::to_string(Case) + " was successful\n";
    }
    return RunVariableTypeScript(Script.c_str(), Expected.c_str());
}

BOOLEAN
TestScriptEngineVariableTypes()
{
    return TestVariableTypeIrContract() &&
           RunVariableTypeScript("{ char narrow = 0xff; unsigned char narrowValue = 0x12345; int promoted = narrowValue; printf(\"%lld %lld %lld\\n\", narrow, narrowValue, promoted); }",
                                 "-1 69 69\n") &&
           RunVariableTypeScript("{ int value = 0n10; value %= 4; long wide = 0n10; wide %= 4; printf(\"%lld %lld\\n\", value, wide); }",
                                 "2 2\n") &&
           RunVariableTypeScript("{ printf(\"%lld %lld %lld %lld %lld\\n\", sizeof(char), sizeof(short), sizeof(int), sizeof(long), sizeof(long long)); }",
                                 "1 2 4 8 8\n") &&
           RunVariableTypeScript("{ printf(\"%lld %lld\\n\", sizeof(!(char)0), sizeof(!(long)0)); }",
                                 "4 4\n") &&
           RunVariableTypeScript("{ if (sizeof(char) == 1 && (int)1 == 1) { printf(\"boolean type syntax\\n\"); } }",
                                 "boolean type syntax\n") &&
           RunVariableTypeScript("{ struct Pair { int left; unsigned short right; }; printf(\"%lld\\n\", sizeof(struct Pair)); }",
                                 "8\n") &&
           RunVariableTypeScript("{ struct NarrowFields { char signedValue; unsigned char unsignedValue; }; struct NarrowFields fields; fields.signedValue = 0xff; fields.unsignedValue = 0x1ff; printf(\"%lld %lld\\n\", fields.signedValue, fields.unsignedValue); }",
                                 "-1 255\n") &&
           RunVariableTypeScript("{ struct CompoundFields { int signedValue; unsigned char unsignedValue; }; struct CompoundFields fields; fields.signedValue = 0n10; fields.signedValue %= 4; fields.unsignedValue = 0x105; fields.unsignedValue += 0x100; printf(\"%lld %lld\\n\", fields.signedValue, fields.unsignedValue); }",
                                 "2 5\n") &&
           RunVariableTypeScript("{ char values[2] = {0xff, 0x7f}; printf(\"%lld %lld\\n\", values[0], values[1]); }",
                                 "-1 127\n") &&
           RunVariableTypeScript("{ typedef unsigned short word; word value = 0x12345; printf(\"%lld %lld\\n\", value, sizeof(word)); }",
                                 "9029 2\n") &&
           RunVariableTypeScript("{ int probe() { printf(\"evaluated\\n\"); return 1; } printf(\"%lld\\n\", sizeof(probe())); }",
                                 "4\n") &&
           RunVariableTypeScript("{ int add_to_55(int value) { return 0n55 + value; } result = add_to_55(0n47); printf(\"%lld\\n\", result); }",
                                 "102\n") &&
           RunVariableTypeScript("{ int typed_factorial(int value) { if (value == 0 || value == 1) { return 1; } return value * typed_factorial(value - 1); } result = typed_factorial(0n10); printf(\"%lld\\n\", result); }",
                                 "3628800\n") &&
           RunVariableTypeScript("{ source_value = 0n123456; destination_value = 0; memcpy(&destination_value, &source_value, 8); printf(\"%lld\\n\", destination_value); }",
                                 "123456\n") &&
           RunVariableTypeScript("{ local_value = 0n55; result = &local_value; printf(\"%lld %lld\\n\", sizeof(result), dq(result)); }",
                                 "8 55\n") &&
           RunVariableTypeScript("{ .global_value = 0n55; result = &(.global_value); printf(\"%lld %lld\\n\", sizeof(result), dq(result)); }",
                                 "8 55\n") &&
           RunVariableTypeScript("{ char narrow_value = 0x7f; char *narrow_pointer = &narrow_value; printf(\"%lld\\n\", *narrow_pointer); }",
                                 "127\n") &&
           RunVariableTypeScript("{ int integerValue = (int)(unsigned char)0x12345; double doubleValue = (double)integerValue; long restored = (long)doubleValue; printf(\"%lld %f %lld\\n\", integerValue, doubleValue, restored); }",
                                 "69 69.000000 69\n") &&
           RunVariableTypeScript("{ char *left = (char *)0xffffffffffffffff; char *right = (char *)0xfffffffffffffff0; if (left > right) { printf(\"%lld %lld 1\\n\", left - right, right - left); } }",
                                 "15 -15 1\n") &&
           RunVariableTypeScript("{ char *base = (char *)0x1000; char *next = base + 0n2; printf(\"%lld\\n\", next - base); }",
                                 "2\n") &&
           RunVariableTypeScript("{ char text[] = \"123456\"; printf(\"%lld %lld %lld %lld %lld\\n\", strlen(text), sizeof(text), text[0], text[5], text[6]); }",
                                 "6 7 49 54 0\n") &&
           RunVariableTypeScript("{ wchar_t text[] = L\"ABCD\"; printf(\"%lld %lld %lld %lld %lld %lld\\n\", wcslen(text), sizeof(text), sizeof(text[0]), text[0], text[3], text[4]); }",
                                 "4 10 2 65 68 0\n") &&
           RunVariableTypeScript("{ do { char first_text[] = \"123456\"; printf(\"%lld\\n\", strlen(first_text)); } while (0); do { wchar_t second_text[] = L\"ABCD\"; printf(\"%lld %lld %lld %lld %lld %lld\\n\", wcslen(second_text), sizeof(second_text), sizeof(second_text[0]), second_text[0], second_text[3], second_text[4]); } while (0); }",
                                 "6\n4 10 2 65 68 0\n") &&
           RunVariableTypeScript("{ int local_strings() { char text[] = \"abc\"; wchar_t wide[] = L\"de\"; text[0] = 'A'; wide[0] = L'D'; return strlen(text) + wcslen(wide) + text[0] + wide[0]; } printf(\"%lld\\n\", local_strings()); }",
                                 "138\n") &&
           RunVariableTypeScript("{ char text[8] = \"abc\"; wchar_t wide[8] = L\"abc\"; char empty[4]; wchar_t wide_empty[4]; printf(\"%lld %lld %lld %lld %lld %lld\\n\", text[7], wide[7], empty[0], empty[3], wide_empty[0], wide_empty[3]); }",
                                 "0 0 0 0 0 0\n") &&
           RunVariableTypeScript("{ char first = 'A'; wchar_t wide_first = L'A'; char text[] = {first, first + 1, 0}; wchar_t wide[] = {wide_first, wide_first + 1, 0}; printf(\"%lld %lld %lld %lld %lld %lld\\n\", sizeof(text), sizeof(wide), text[0], text[1], wide[0], wide[1]); }",
                                 "3 6 65 66 65 66\n") &&
           RunVariableTypeScript("{ printf(\"%lld %lld %lld %lld\\n\", '\\n', '\\x41', L'\\n', L'\\x0041'); }",
                                 "10 65 10 65\n") &&
           RunVariableTypeScript("{ char text[] = {0x1ff, 0}; wchar_t wide[] = {0x12345, 0}; char *p = text; wchar_t *wp = wide; p[0] = 'W'; *(p + 1) = 'X'; wp[0] = L'Y'; *(wp + 1) = L'Z'; printf(\"%lld %lld %lld %lld %lld %lld\\n\", text[0], text[1], wide[0], wide[1], p + 1 - p, wp + 1 - wp); }",
                                 "87 88 89 90 1 1\n") &&
           RunVariableTypeScript("{ char text[] = \"123456\"; wchar_t wide[] = L\"123456\"; char *p = text; wchar_t *wp = wide; printf(\"%lld %lld %lld %lld\\n\", strlen(p), strlen(text + 2), wcslen(wp), wcslen(wide + 2)); }",
                                 "6 4 6 4\n") &&
           RunVariableTypeScript("{ if (1 || 1 / 0) { printf(\"or short circuit\\n\"); } if (0 && 1 / 0) { printf(\"unexpected\\n\"); } printf(\"and short circuit\\n\"); }",
                                 "or short circuit\nand short circuit\n") &&
           RunVariableTypeScript("{ inferred = 0xffffffffffffffff; printf(\"%llu %lld\\n\", inferred, sizeof(inferred)); }",
                                 "18446744073709551615 8\n") &&
           RunVariableTypeScript("{ .manual_pointer_value = 0x55; manual_pointer = & .manual_pointer_value; printf(\"%llx %llx\\n\", dq(manual_pointer), *manual_pointer); }",
                                 "55 55\n") &&
           TestManualSemanticCase56() &&
           TestVariableTypeSemanticCase88() &&
           TestManualSemanticCases40To49() &&
           TestStringVariableSemanticFile() &&
           RunVariableTypeScriptExpectFailure("{ unsigned float invalidValue = 1.0; }") &&
           RunVariableTypeScriptExpectFailure("{ long double invalidValue = 1.0; }") &&
           RunVariableTypeScriptExpectFailure("{ long long long invalidValue = 1; }") &&
           RunVariableTypeScriptExpectFailure("{ char text[3] = \"abcd\"; }") &&
           RunVariableTypeScriptExpectFailure("{ wchar_t text[3] = L\"abcd\"; }") &&
           RunVariableTypeScriptExpectFailure("{ char text[] = {'a', 'b', 0}; char bad[2] = {'a', 'b', 0}; }") &&
           RunVariableTypeScriptExpectFailure("{ char value = ''; }") &&
           RunVariableTypeScriptExpectFailure("{ char value = 'ab'; }") &&
           RunVariableTypeScriptExpectFailure("{ char value = '\\q'; }") &&
           RunVariableTypeScriptExpectFailure("{ wchar_t value = L'\\x12345'; }") &&
           RunVariableTypeScriptExpectRuntimeFailure("{ char text[1]; char *pointer = text; *(pointer + 0x10000) = 'X'; }") &&
           RunVariableTypeScriptExpectFailure("{ local_value = 0n55; unsigned long long explicit_result = &local_value; }") &&
           RunVariableTypeScriptExpectFailure("{ int value = 1; value = value / 0; }") &&
           RunVariableTypeScriptExpectFailure("{ unsigned long value = 1; value = value << 0n64; }");
}
