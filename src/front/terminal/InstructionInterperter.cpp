#include "InstructionInterperter.h"

#include <iostream>

namespace src::front::terminal {
    vector<ParameterType> InstructionInterpreter::ParamTypes={};
    vector<InstructionType> InstructionInterpreter::InstTypes={};
    vector<DataTypeInfo> InstructionInterpreter::TypesInfo={};
    void InstructionInterpreter::Setup() {
        vector<string> list = {
                "-1:h/help|",
                "-1:q/exit|",
                "-1:scx/show-context|",
                "-1:ecx/exit-context|",
                "0:ss/start-server|-sn %s/--serverName %s",
                "0:cc/connect-client|-sa %s/--serverAddress %s",
                "1:sd/shutdown|",
                "1:sl/show-log|",
                "3:ccr/change-chat-room|-i %i/--roomID %i,-n %s/--roomName %s",
                "3:msgin/messageIn|-i %i/--roomID %i,-n %s/--roomName %s|-mc %s/--messageContent %s",
                "5:msg/message|-m %s/--message %s",
                "2:li/login|-i %i/--ID %i|-k %s/--loginKey %s",
                "3:lo/logout|",
                "3:mcr/make-chat-room|-n %s/--name %s|-i %i[]/--clientIDs %i[]",
                "4:kcr/kill-chat-room|-i %i/-roomID %i",
                "4:ler/leave-room|-i %i/--roomID %i,-n %s/--roomName %s",
                "5:kr/kick-from-room|-i %i[]/--kickClientIDs %i[]",
                "2:reg/register|-n %s/--clientDisplayName %s|-k %s/--regClientKey %s",
                "4:sf/save-friend|-i %i/--clientID %i",
                "4:delf/delete-friend|-i %o/--clientID %i",
                "3:pfl/print-friend-list|",
                "3:accd/account-details|safe,unsafe"
        };

        TypesInfo = {
                DataTypeInfo("%i", "ID", "0123456789", "0123456789", " "),
                DataTypeInfo("%i[]", "IDList", "0123456789,", "[", "]"),
                DataTypeInfo("%s", "String", "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789 .", "\"",
                             "\"")
        };


        for (const string &cur: list) {
            InstructionType toPush = ProcessInstruction(cur);
            if (toPush.LongForm.empty())
                std::cerr << "Parse of: '" << cur << "' has failed!" << "\n";
            else
                InstTypes.push_back(toPush);
        }
    }

    InstructionType InstructionInterpreter::ProcessInstruction(const string &inp) {
        int i, j;
        bool stop = false;
        InstructionType res = {};

        //region SerializeContext
        {
            for (i = 0; i < inp.size() && !stop; stop = inp[i++] == ':');
            if (!stop)
                return {};
            int contID = std::stoi(inp.substr(0, i - 1));
            if (!(contID > -2 && contID < 6))
                return {};
            res.ValidContext = static_cast<Context>(contID);
        }
        //endregion


        //region GetForms
        {
            stop = false;
            j = i;
            for (; i < inp.size() && !stop; stop = inp[i++] == '|');
            if (!stop)
                return {};
            string names = inp.substr(j, i - j - 1);

            //Split names:
            stop = false;
            for (j = 0; j < names.size() && !stop; stop = names[j++] == '/');
            if (!stop)
                return {};
            res.ShortForm = names.substr(0, j - 1);
            res.LongForm = names.substr(j);
        }
        //endregion

        //Exit if instruction requires no parameters
        if (inp.size() == i)
            return res;

        //region Params
        {
            string paramSpace = inp.substr(i, inp.size() - i);
            //region SeparateCategories
            vector<string> categories;
            stop = false;
            string current = string(paramSpace);
            do {
                stop = false;
                for (i = 0; i < current.size() && !stop; stop = current[i++] == '|');
                categories.push_back(current.substr(0, (stop) ? i - 1 : i));
                current = current.substr(i);
            } while (stop);
            //endregion
            j = 0;
            for (const string &cat: categories) {
                //region SeparateParams
                vector<string> params;
                stop = false;
                current = string(cat);
                do {
                    stop = false;
                    for (i = 0; i < current.size() && !stop; stop = current[i++] == ',');
                    params.push_back(current.substr(0, (stop) ? i - 1 : i));
                    current = current.substr(i);
                } while (stop);
                //endregion

                for (const string &param: params) {
                    //Search for '-'
                    stop = false;
                    for (i = 0; i < param.size() && !stop; stop = param[i++] == '-');
                    ParameterType cur;
                    if (!stop) {
                        cur.TypeDescriptor = "static";
                        cur.LongForm = string(param);
                        cur.ShortForm = string(param);
                    } else {
                        stop = false;
                        for (i = 0; i < param.size() && !stop; stop = param[i++] == '/');
                        string sSec = param.substr(0, i - 1);
                        string lSec = param.substr(i);

                        stop = false;
                        for (i = 0; i < sSec.size() && !stop; stop = sSec[i++] == ' ');
                        cur.ShortForm = string(sSec.substr(0, i - 1));

                        stop = false;
                        for (i = 0; i < lSec.size() && !stop; stop = lSec[i++] == ' ');
                        cur.LongForm = lSec.substr(0, i - 1);

                        cur.TypeDescriptor = string(lSec.substr(i));
                    }

                    cur.CategoryIndex = j;

                    stop = false;
                    for (i = 0; i < ParamTypes.size() && !stop; stop = ParamTypes[i++].LongForm == cur.LongForm);
                    if (!stop) {
                        ParamTypes.push_back(cur);
                        i = (int) ParamTypes.size();
                    }
                    res.Parameters.push_back(ParamTypes[i - 1]);
                }
                j++;
            }
        }
        //endregion
        int paramCount = 0;
        for (i = 0; i < inp.size(); paramCount = (inp[i++] == '|') ? paramCount + 1 : paramCount);
        res.requiredParamCount = paramCount;
        return res;
    }

    Instruction InstructionInterpreter::Parse(const string &inp) {
        string checkRes = IsLegalInst(inp);
        if (!checkRes.empty())
            throw invalid_argument(checkRes);

        Instruction res = {};

        string instName;
        int i, j;
        bool stop = false;

        //Get instruction name
        for (i = 0; i < inp.size() && !stop; stop = inp[i++] == ' ');
        instName = inp.substr(0, (stop) ? i - 1 : i);

        //region GetInstructionType
        stop = false;
        for (i = 0; i < InstTypes.size() && !stop; stop = (InstTypes[i].ShortForm == instName ||
                                                           InstTypes[i].LongForm == instName), i++);
        InstructionType *instType = &InstTypes[i - 1];
        //endregion

        res.Type = instType;


        //region Handle parameters:
        if (!instType->Parameters.empty()) {
            if (instType->Parameters[0].TypeDescriptor == "static") {
                string param;
                stop = false;
                for (i = 0; i < inp.size() && !stop; stop = inp[i++] == ' ');
                param = inp.substr(i);
                j = 0;
                for (const auto &sParam: instType->Parameters)
                    if (sParam.ShortForm == param)
                        break;
                    else
                        ++j;
                auto parameter = Parameter{};
                parameter.Type = &instType->Parameters[j];
                parameter.Value = param;
                res.Params.push_back(parameter);
            } else {
                string param;
                stop = false;
                for (i = 0; i < inp.size() && !stop; stop = inp[i++] == ' ');
                param = inp.substr(i);
                i = 0;
                for (int paramIndex = 0; paramIndex < instType->requiredParamCount; paramIndex++) {
                    ParameterType *found = nullptr;
                    bool flag = false;
                    int saveI = i;
                    for (auto &Parameter: instType->Parameters) {
                        ParameterType *cur = &Parameter;
                        i = saveI;
                        for (stop = false; i < param.size() && !stop; stop = param[i++] == '-');

                        flag = stop;
                        stop = false;
                        for (j = i; j < param.size() && !stop; stop = param[j++] == ' ');
                        if (!stop)
                            --j;
                        stop = false;
                        if (param.substr(i - 1, j - i) == cur->ShortForm ||
                            param.substr(i, i - j) == cur->LongForm) {
                            found = cur;
                            stop = true;
                            break;
                        }
                    }

                    // j points to the opener char of the input data.
                    int k;
                    for (k = 0, flag = false;
                         k < TypesInfo.size() && !flag; flag = TypesInfo[k++].Descriptor == found->TypeDescriptor);
                    DataTypeInfo typeInfo = TypesInfo[k - 1];
                    //Check if the opener char is valid:
                    for (k = 0, flag = false; k < typeInfo.Opener.size() && !flag;
                         flag = typeInfo.Opener[k++] == param[j]);

                    int curI = (typeInfo.Descriptor == "%i") ? j : j + 1;
                    for (flag = false; curI < param.size() && !flag; curI++)
                        flag = param[curI] == typeInfo.Closer[0];

                    auto curParameter = Parameter{};
                    curParameter.Type = found;
                    if (curParameter.Type->TypeDescriptor == "%i") {
                        unsigned long long id = std::stoull(param.substr(j, curI - j));
                        curParameter.Value = id;
                    } else if (curParameter.Type->TypeDescriptor == "%s") {
                        string name = param.substr(j + 1, curI - j - 2);
                        curParameter.Value = name;
                    }
                    else if (curParameter.Type->TypeDescriptor == "%i[]") {
                        ++j;
                        vector<unsigned long long> ids = {};
                        stop = false;
                        curI = j - 1;
                        do {
                            flag = false;
                            for (; !flag && !stop; flag = param[curI] == ',', stop = param[curI] == ']', curI++);
                            unsigned long long id = std::stoull(param.substr(j, curI - j));
                            ids.push_back(id);
                            j = curI;
                        } while (!stop);
                        curParameter.Value = ids;
                    }
                    res.Params.push_back(curParameter);
                }
            }
        }
        //endregion

        return res;
    }

    string InstructionInterpreter::IsLegalInst(const string &inp) {
        string instName;
        int i, j;
        bool stop = false;

        // Get instruction name
        for (i = 0; i < inp.size() && !stop; stop = inp[i++] == ' ');
        instName = inp.substr(0, (stop) ? i - 1 : i);

        // region GetInstructionType
        stop = false;
        for (i = 0; i < InstTypes.size() && !stop; stop = (InstTypes[i].ShortForm == instName ||
                                                           InstTypes[i].LongForm == instName), i++);
        if (!stop)
            return "Unknown instruction '" + instName + "'";

        InstructionType &instType = InstTypes[i - 1];
        // endregion

        if (instType.Parameters.size() > 1 && instType.Parameters[0].TypeDescriptor == "static") {
            string param;
            stop = false;
            for (i = 0; i < inp.size() && !stop; stop = inp[i++] == ' ');
            param = inp.substr(i);
            stop = false;
            for (const auto &sParam: instType.Parameters)
                if ((stop = sParam.ShortForm == param))
                    break;
            if (!stop)
                return "Invalid static parameter '" + param + "' for instruction '" + instName + "'";

        } else if (!instType.Parameters.empty()) {
            string param;
            stop = false;
            for (i = 0; i < inp.size() && !stop; stop = inp[i++] == ' ');
            param = inp.substr(i);
            i = 0;
            vector<int> catParamCount;
            for (int paramIndex = 0;
                 paramIndex < instType.requiredParamCount; catParamCount.push_back(0), paramIndex++);
            for (int paramIndex = 0; paramIndex < catParamCount.size(); paramIndex++) {
                ParameterType found = {};
                bool flag = false;
                int saveI = i;
                for (const auto &cur: instType.Parameters) {
                    i = saveI;
                    for (stop = false; i < param.size() && !stop; stop = param[i++] == '-');
                    if (!stop)
                        return "No param identifier found!";
                    flag = stop;
                    stop = false;
                    for (j = i; j < param.size() && !stop; stop = param[j++] == ' ');
                    if (!stop)
                        --j;
                    stop = false;
                    if (param.substr(i - 1, j - i) == cur.ShortForm ||
                        param.substr(i, i - j) == cur.LongForm) {
                        found = cur;
                        stop = true;
                        break;
                    }
                }
                if (!stop)
                    return "Unidentified input parameter: '" + param.substr(i - 1, j - i) + "'";
                // j points to the opener char of the input data.
                int k;
                for (k = 0, flag = false;
                     k < TypesInfo.size() && !flag; flag = TypesInfo[k++].Descriptor == found.TypeDescriptor);
                if (!flag)
                    return "CRITICAL: Invalid data type detected!";
                DataTypeInfo typeInfo = TypesInfo[k - 1];
                catParamCount[found.CategoryIndex]++;
                for (flag = false, k = 0; k < catParamCount.size() && !flag; flag = catParamCount[k++] >= 2);
                if (flag)
                    return "Detected input for multiple parameters from the same category!";
                // Check if the opener char is valid:
                for (k = 0, flag = false; k < typeInfo.Opener.size() && !flag; flag = typeInfo.Opener[k++] == param[j]);
                if (!flag)
                    return "Opening char of data input type '" + typeInfo.Descriptor +
                           "' is invalid. Valid opening chars are '" + typeInfo.Opener + "'";
                {
                    int curI = (typeInfo.Descriptor == "%i") ? j : j + 1;
                    bool closedFlag = false;
                    // Flag signifies the appearance of an illegal char:
                    for (flag = false; curI < param.size() && !closedFlag && !flag; curI++) {
                        if((closedFlag = param[curI] == typeInfo.Closer[0]))
                            break;
                        for (k = 0, flag = true;
                             k < typeInfo.ValidChars.size() && flag && !closedFlag; flag = param[curI] !=
                                                                                           typeInfo.ValidChars[k++]);
                    }
                    flag = (closedFlag) ? false : closedFlag;
                    if (typeInfo.Closer != " " && !closedFlag)
                        return "Parameter input was not properly closed! \nMissing: '" + typeInfo.Closer + "'";
                }
                if (flag)
                    return "An invalid char was detected inside data input for type '" + typeInfo.Descriptor + "'";

            }
            bool flag;
            for (flag = false, i = 0; i < catParamCount.size() && !flag; flag = catParamCount[i++] != 1);
            if (flag)
                return "Instruction is missing a parameter from one of its categories";
        } else {
            string reducedInst;
            for (i = 0; i < inp.size(); i++)
                if (inp[i] != ' ' && inp[i] != '\n')
                    reducedInst += inp[i];
            if (reducedInst != instType.ShortForm && reducedInst != instType.LongForm)
                return "Instruction '" + instType.LongForm + "' doesn't accept any parameters";
        }
        return "";
    }
} // terminal