#ifndef exit_code
#define exit_code

enum ExitCode {
    ExitCodeSuccess = 0,
    ExitCodeCouldNotReadAsmFile,
    ExitCodeCouldNotWriteBinFile,
    ExitCodeCouldNotWriteSymbolsFile,
    ExitCodeResultProgramEmpty,
    ExitCodeProgramArgumentsInvalid,
    ExitCodeNumberLiteralOutOutRange,
    ExitCodeCharacterLiteralOutOutRange,
    ExitCodeLabelNameTooLong,
    ExitCodeInvalidLabelName,
    ExitCodeLabelNameNotUnique,
    ExitCodeInvalidNumberLiteral,
    ExitCodeInvalidEscapeSequence,
    ExitCodeUnterminatedString,
    ExitCodeDeclaringValueOutOfMemoryRange,
    ExitCodeMemoryValueOverridden,
    ExitCodeTooManyLabelDefinitions,
    ExitCodeTooManyLabelUses,
    ExitCodeTooManyImmediateValueUses,
    ExitCodeReferenceToInvalidAddress,
    ExitCodeOriginOutOfMemoryRange,
    ExitCodeInvalidInstructionArgument,
    ExitCodeInvalidDirectiveArgument,
    ExitCodeInvalidToken,
    ExitCodeInvalidCharacterLiteral,
    ExitCodeUndefinedLabel,
    ExitCodeUnexpectedEndOfFile,
    ExitCodeImmediateValueDeclarationOutOfMemoryRange,
    ExitCodeMissingComma,
    ExitCodeNameCollision,
    ExitCodeUnexpectedEndOfMacro,
    ExitCodeTooManyMacros,
    ExitCodeTooManyMacroParams,
    ExitCodeInvalidMacroArgumentsCount
};

#endif