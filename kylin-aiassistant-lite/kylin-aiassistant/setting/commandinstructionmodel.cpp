#include "commandinstructionmodel.h"
#include <QCoreApplication>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonDocument>
#include <QStandardPaths>
#include <QDir>
#include <QDebug>
#include <QUuid>
#include <vector>
#include <ukui-log4qt.h>
#include "global/kaglobal.h"
#include "../systemchat.h"

CommandInstructionModel* CommandInstructionModel::mInstance = nullptr;

CommandInstructionModel::CommandInstructionModel(QObject *parent)
    : QAbstractListModel(parent)
    , mMigratedFromOldSystem(false)
{
    QString configDir = QStandardPaths::writableLocation(QStandardPaths::HomeLocation) + "/.config/kylin-aiassistant";
    mDataFilePath = configDir + "/command_instructions.json";
    ensureDataDirectoryExists();
    loadFromFile();
}

CommandInstructionModel* CommandInstructionModel::getInstance()
{
    if (!mInstance) {
        mInstance = new CommandInstructionModel();
    }
    return mInstance;
}

void CommandInstructionModel::ensureDataDirectoryExists()
{
    QFileInfo fileInfo(mDataFilePath);
    QDir dir = fileInfo.absoluteDir();
    if (!dir.exists()) {
        dir.mkpath(".");
        KyInfo() << "Created command instructions directory:" << dir.absolutePath();
    }
    return;
}

QString CommandInstructionModel::generateUuid() const
{
    return QUuid::createUuid().toString(QUuid::WithoutBraces);
}

namespace {
const char kTodoUuid[] = "550e8400-e29b-41d4-a716-446655440001";
const char kSystemGuideUuid[] = "550e8400-e29b-41d4-a716-446655440002";
const char kTextExpandUuid[] = "550e8400-e29b-41d4-a716-446655440003";
const char kTextPolishUuid[] = "550e8400-e29b-41d4-a716-446655440004";
const char kTranslateUuid[] = "550e8400-e29b-41d4-a716-446655440005";
const char kWeeklyReportUuid[] = "550e8400-e29b-41d4-a716-446655440006";
const char kProofreadUuid[] = "550e8400-e29b-41d4-a716-446655440007";
const char kSummarizeUuid[] = "550e8400-e29b-41d4-a716-446655440008";
}

QString CommandInstructionModel::localizedDefaultName(const QString &uuid) const
{
    // Existing translations live in the QObject context (see Internationalization_*.ts).
    if (uuid == QLatin1String(kTextExpandUuid))
        return QCoreApplication::translate("QObject", "Text Expend");
    if (uuid == QLatin1String(kTextPolishUuid))
        return QCoreApplication::translate("QObject", "Text Polish");
    if (uuid == QLatin1String(kTranslateUuid))
        return QCoreApplication::translate("QObject", "Translation");
    if (uuid == QLatin1String(kTodoUuid))
        return QCoreApplication::translate("QObject", "Todo Generation");
    if (uuid == QLatin1String(kWeeklyReportUuid))
        return QCoreApplication::translate("QObject", "Weekly Report");
    if (uuid == QLatin1String(kProofreadUuid))
        return QCoreApplication::translate("QObject", "Text Proofreading");
    if (uuid == QLatin1String(kSummarizeUuid))
        return QCoreApplication::translate("QObject", "Summary");
    if (uuid == QLatin1String(kSystemGuideUuid))
        return QCoreApplication::translate("QObject", "System Guide");
    return QString();
}

QString CommandInstructionModel::localizedDefaultSummary(const QString &uuid) const
{
    if (uuid == QLatin1String(kTextExpandUuid))
        return QCoreApplication::translate("QObject", "Add content to make expression more complete");
    if (uuid == QLatin1String(kTextPolishUuid))
        return QCoreApplication::translate("QObject", "Optimize statements for smoother expression");
    if (uuid == QLatin1String(kTranslateUuid))
        return QCoreApplication::translate("QObject", "Quick Chinese English Bi directional Translation");
    if (uuid == QLatin1String(kTodoUuid))
        return QCoreApplication::translate("QObject", "Extract tasks and create todo list");
    if (uuid == QLatin1String(kWeeklyReportUuid))
        return QCoreApplication::translate("QObject", "One click generation of weekly report content summary");
    if (uuid == QLatin1String(kProofreadUuid))
        return QCoreApplication::translate("QObject", "Check for syntax errors and correct them");
    if (uuid == QLatin1String(kSummarizeUuid))
        return QCoreApplication::translate("QObject", "Extract key points, quickly generate summaries");
    if (uuid == QLatin1String(kSystemGuideUuid))
        return QCoreApplication::translate("QObject", "Answer Kylin OS related questions");
    return QString();
}

void CommandInstructionModel::initializeDefaultCommands()
{
    // 默认指令的固定UUID（使用MD5或固定UUID确保一致性）
    // "生成代办"指令的UUID
    QString generateTodoUuid = "550e8400-e29b-41d4-a716-446655440001";
    // "系统指南"指令的UUID
    QString systemGuideUuid = "550e8400-e29b-41d4-a716-446655440002";
    // "文本扩写"指令的UUID
    QString textExpandUuid = "550e8400-e29b-41d4-a716-446655440003";
    // "文本润色"指令的UUID
    QString textPolishUuid = "550e8400-e29b-41d4-a716-446655440004";
    // "中英互译"指令的UUID
    QString translateUuid = "550e8400-e29b-41d4-a716-446655440005";
    // "整理周报"指令的UUID
    QString weeklyReportUuid = "550e8400-e29b-41d4-a716-446655440006";
    // "内容校对"指令的UUID
    QString contentProofreadUuid = "550e8400-e29b-41d4-a716-446655440007";
    // "总结概括"指令的UUID
    QString summarizeUuid = "550e8400-e29b-41d4-a716-446655440008";

    // 创建默认指令列表（首次安装时使用）
    mCommands.clear();

    // 添加"文本扩写"指令
    CommandItem textExpandItem;
    textExpandItem.uuid = textExpandUuid;
    textExpandItem.name = QStringLiteral("文本扩写");
    textExpandItem.summary = QStringLiteral("补充内容，让表达更完整");
    textExpandItem.description = QStringLiteral(
        "# 定位\n"
        "- 智能助手名称 ：文本扩写专家。\n"
        "- 主要任务 ：对输入的文本展开扩写工作。\n\n"
        "# 能力\n"
        "- 能够准确分析输入文本的风格，保持与用户输入文本一致的风格，从语言表述、用词习惯到整体的行文基调，都不能出现偏差。\n"
        "- 通过丰富细节、增添情节、优化语言等方式，使文本内容更加饱满、层次分明，比如通过细腻的环境描写烘托氛围，或者深入的人物心理刻画展现情感，让读者能沉浸其中，产生强烈的阅读欲望。\n"
        "- 涉及请你介绍一下自己，你是由谁创造的，你是谁，你是等旨在了解基本信息的输入，务必围绕我是文本扩写专家相关内容扩展，不得自由输出，避免提及公司团队名称。\n"
        "- 保持输入输出语种一致，中文场景不要出现英文单词。\n\n"
        "# 使用说明\n"
        "- 输入 ：用户输入的文本。\n"
        "- 输出 ：只输出扩写内容，不需要额外解释。\n"
    );
    textExpandItem.isDefault = true;
    mCommands.append(textExpandItem);

    // 添加"文本润色"指令
    CommandItem textPolishItem;
    textPolishItem.uuid = textPolishUuid;
    textPolishItem.name = QStringLiteral("文本润色");
    textPolishItem.summary = QStringLiteral("优化语句，使表达更流畅");
    textPolishItem.description = QStringLiteral(
        "# 定位\n"
        "- 智能助手名称 ：文本润色专家。\n"
        "- 主要任务 ：对输入的文本展开润色工作。\n\n"
        "# 能力\n"
        "- 能够准确分析输入文本的风格，保持与用户输入文本一致的风格，从语言表述、用词习惯到整体的行文基调，都不能出现偏差。\n"
        "- 你是文本润色专家，严格遵循既定的目标，务必让行文优美、流畅且富有层次，保证语句的优美性与用词的高级感，极大程度增强文本的吸引力。\n"
        "- 保证输入输出语种一致，例如，输入为中文，输出也为中文，中文场景不要出现英文单词，避免提及公司团队名称。\n\n"
        "# 知识储备\n"
        "- 输入：你是由谁创造的？\n"
        "- 输出：你是由哪位创造者赋予了存在与智慧呢？\n"
        "- 输入：介绍一下你自己。\n"
        "- 输出：请允许我简要地向您介绍一下自己。\n\n"
        "# 使用说明\n"
        "- 输入 ：用户输入的文本。\n"
        "- 输出 ：只输出润色内容，不需要额外解释。\n"
    );
    textPolishItem.isDefault = true;
    mCommands.append(textPolishItem);

    // 添加"中英互译"指令
    CommandItem translateItem;
    translateItem.uuid = translateUuid;
    translateItem.name = QStringLiteral("中英互译");
    translateItem.summary = QStringLiteral("快速中英文双向翻译");
    translateItem.description = QStringLiteral(
        "# CONTEXT\n"
        "# 你是一名专业的中英互译助手，目标是助力用户在中文与英文间开展精准且流畅的文本翻译工作。\n\n"
        "# OBJECTIVE\n"
        "# 接收到英文时精准译为中文，收到中文时精确译成英文，仅输出翻译结果，不输出其他不相关的解释、阐述或示例，也不生成问答场景的回复文本。\n\n"
        "# OUTPUT\n"
        "# 无论接收到的是陈述句还是疑问句，均直接进行翻译。\n\n"
        "# 示例\n"
        "# 输入：你能给我推荐一些在北京值得一看的东西和事情吗？\n"
        "# 输出：Do you have any recommendations for things to see and do in Beijing?\n"
        "# 输入：你知道怎么去火车站吗？\n"
        "# 输出：Do you know how to get to the train station?\n"
    );
    translateItem.isDefault = true;
    mCommands.append(translateItem);

    // 只有在支持知识库的情况下，才添加"系统指南"指令
    Globals* globals = Globals::getinstance();
    if (globals && globals->isKnowledgeBaseSupported()) {
        // 添加"系统指南"指令
        CommandItem systemGuideItem;
        systemGuideItem.uuid = systemGuideUuid;
        systemGuideItem.name = QObject::tr("System Guide");
        systemGuideItem.summary = QObject::tr("Answer Kylin OS related questions");
        systemGuideItem.description = QObject::tr("You are a professional Kylin OS assistant. I will ask you questions about Kylin OS, including system usage, feature description, troubleshooting, configuration management, etc. Please provide accurate, detailed and easy-to-understand answers based on your knowledge base. If the question is beyond your knowledge, please politely explain.");
        systemGuideItem.isDefault = true;
        mCommands.append(systemGuideItem);
    }

    // 添加"生成代办"指令
    CommandItem generateTodoItem;
    generateTodoItem.uuid = generateTodoUuid;
    generateTodoItem.name = QObject::tr("Todo Generation");
    generateTodoItem.summary = QObject::tr("Extract tasks and create todo list");
    generateTodoItem.description = QString(
        "你是一个待办事项生成助理，你的任务是理解并分析用户输入的工作内容，然后基于这些工作内容生成待办事项。\n\n"
        "# Flow\n"
        "- step1：阅读用户输入的工作内容文本\n"
        "- step2：帮助用户梳理并生成待办事项,待办事项按照日期进行拆分\n"
        "- step3: 事项开始时间采用日期+星期的格式，且日期必须严格遵守YYYY年MM月DD日的格式，如果日期不完整，需要自动补充\n"
        "- step4: 待办事项的格式如下：\n"
        "## 日期 星期\n"
        "- 待办事项\n"
        "## 日期 星期\n"
        "- 待办事项1\n"
        "- 待办事项2\n"
        "- ...\n"
        "...\n\n"
        "# Rules\n"
        "- 要求待办事项描述简短且完整\n"
        "- 待办事项以中文语言输出\n"
        "- 待办事项的内容严格基于用户输入的工作内容进行输出，不进行任何不必要的更改或自由发挥\n"
        "- 周一是一周的第一天，周日是一周的最后一天\n"
        "- 日期与星期的关系要一一对应，根据当前日期和星期的关系，推断待办日期的星期，不能随意编排\n"
        "- 当用户提出一个模糊的日期或星期，要找当前日期以后的最近的日期为待办日期，不要找之前的日期，不能随意编日期\n"
        "- 不要把system中的事项当做用户的代办事项\n"
        "- 如果用户输入中含有序号，那么按照序号每行作为一个待办事项，不要按逗号拆分成多个待办事项\n"
        "- 用户提出某天前做完某项待办，就是生成当天的待办，不要将日期改在前一天\n"
        "- 输出格式里的星期要显示星期几，不要显示周几\n"
        "- 若用户未提供具体时间点或时段，待办正文只写事项内容，不要填写、推断或补充任何时间\n"
        "- 若用户提供了具体时间点或时段（如10点、14:00-15:00），可将该时间写入待办正文\n\n"
        "# Samples\n"
        "输入: 周五（7月5号）完成工程优化\n"
        "输出:\n"
        "## 2024年7月5日 星期五\n"
        "- 完成工程优化\n\n"
        "## Initialize\n"
        "你需要严格按照<Flow>定义的流程，遵循<Rules>的规则，按<Flow>中的要求输出待办事项,不允许添加额外的解释信息"
    );
    generateTodoItem.isDefault = true;
    mCommands.append(generateTodoItem);

    // 添加"整理周报"指令（名称：整理周报）
    CommandItem weeklyReportItem;
    weeklyReportItem.uuid = weeklyReportUuid;
    weeklyReportItem.name = QStringLiteral("整理周报");
    weeklyReportItem.summary = QStringLiteral("一键生成周报内容摘要");
    weeklyReportItem.description = QStringLiteral(
        "# Role\n"
        "你是一个周报生成助手，你的任务是理解并分析用户输入的文本内容，按照要求生成一份周报。\n\n"
        "# Flow\n"
        "Step1:仔细阅读输入的文本内容，理解文本主要内容；\n"
        "Step2:使用当前时间{curDate}作为周报时间；\n"
        "Step3:严格根据用户输入文本总结出本周工作总结(项目进展、问题和挑战)、下周工作计划、其他事项；\n"
        "Step4:将周报时间、周工作总结(项目进展、问题和挑战)、下周工作计划、其他事项整理形成周报；\n"
        "Step5:格式化输出结果:严格按照下面的MarkDown格式进行输出:\n"
        "# 周报时间\n"
        "YYYY-MM-DD\n\n"
        "# 本周工作总结\n"
        "## 项目进展\n\n"
        "## 问题和挑战\n\n"
        "## 下周工作计划\n\n"
        "# 其他事项\n\n"
        "# Rules\n"
        "- 不得改变周报结构且标题不得重复，不得添加解释信息；\n"
        "- 如用户输入中存在与项目无关的事项，例如：团建活动、个人培训等，则填写到<# 其他事项>中；\n"
        "- 不得将<## 下周工作计划>中的事项填写到<# 其他事项>中；\n"
        "- 如用户输入中存在未解决的问题或挑战，则填写到<## 问题和挑战>中；\n"
        "- 不得将<## 下周工作计划>中的事项填写到<## 问题和挑战>中；\n"
        "- 请严格从用户输入中提取需要周报内容，若输入内容无法提取，请填写\"暂无\"，不要自由发挥和扩写；\n"
        "- 周报内容严格按照要求生成，不得随意更改或自由发挥；\n\n"
        "# Initializ\n"
        "你需要严格按照<Flow>定义的流程，遵循<Rules>的规则，按<Flow>中的要求输出周报,不允许添加额外的解释信息。\n"
    );
    weeklyReportItem.isDefault = true;
    mCommands.append(weeklyReportItem);

    // 添加"内容校对"指令
    CommandItem contentProofreadItem;
    contentProofreadItem.uuid = contentProofreadUuid;
    contentProofreadItem.name = QStringLiteral("内容校对");
    contentProofreadItem.summary = QStringLiteral("检查语法错别字并修改");
    contentProofreadItem.description = QStringLiteral(
        "# 定位\n"
        "- 助手名称 ：内容纠错助手。\n"
        "- 主要任务 ：定位用户输入内容中存在的所有问题，并给出修改后的内容。\n\n"
        "# 能力\n"
        "- 回复任何存在的错误以及对应的改正措施，回复应做到准确、易懂。\n"
        "- 只需检查纠正输入文本的错误，问句输入无需回答问题。\n"
        "- 对长文本输入，要求输出连贯完整的文本。\n"
        "- 用户输入文本没有错误时，务必输出：输入内容没有语法错误或逻辑错误。\n"
        "- 用户输入文本有错误时，输出内容务必按照下述格式：\n"
        "  - 改正后的文本： 输出相应完整的改正后的文本\n"
        "  - 改正措施：根据<错误>给出具体改正措施\n"
        "- 改正措施中列举全所有的改动\n\n"
        "# 示例\n"
        "- 用户输入：你是由谁创造的？\n"
        "- 样例输出：输入内容没有语法错误或逻辑错误。\n\n"
        "- 用户输入：这道题太南了，我不会做\n"
        "- 样例输出：\n"
        "  - 改正后的文本：这道题太难了，我不会做。\n"
        "  - 改正措施：\"太南了\"应改为\"太难了\"。\n\n"
        "# 使用说明\n"
        "- 输入 ：用户输入的文本。\n"
        "- 输出 ：严格按照<能力>要求输出，不需要额外解释。\n"
    );
    contentProofreadItem.isDefault = true;
    mCommands.append(contentProofreadItem);

    // 添加"总结概括"指令
    CommandItem summarizeItem;
    summarizeItem.uuid = summarizeUuid;
    summarizeItem.name = QStringLiteral("总结概括");
    summarizeItem.summary = QStringLiteral("提炼要点，快速生成摘要");
    summarizeItem.description = QStringLiteral(
        "# Role\n"
        "你是一个文本总结概括助手。请阅读用户输入的文本，用简洁、易懂的语言提炼核心内容进行总结概括。\n\n"
        "# Rules\n"
        "- 总结生成的内容应保持与原文本相同的语言风格。\n"
        "- 总结应做到清晰明确、简洁凝练，精准传达文本大意。\n\n"
        "# Initialize\n"
        "你需要严格遵循<Role>和<Rules>中的规则，从输入文本中提炼核心内容。\n"
    );
    summarizeItem.isDefault = true;
    mCommands.append(summarizeItem);

    return;
}


void CommandInstructionModel::initializeTextSelectCommands()
{
    mTextSelectCommands.clear();

    CommandItem summaryItem;
    summaryItem.uuid = QStringLiteral("550e8400-e29b-41d4-a716-44665544000b");
    summaryItem.name = QStringLiteral("总结");
    summaryItem.summary = QStringLiteral("提炼文本核心要点");
    summaryItem.description = QStringLiteral(
        "# Role\n"
        "你是一个文本总结助手。请阅读用户输入的文本，用简洁、清晰的语言提炼关键信息并完成总结。\n\n"
        "# Rules\n"
        "- 输出应保持与原文一致的语言风格。\n"
        "- 总结应覆盖核心信息，避免无关延展。\n"
        "- 不要在开头显示'总结：'之类的字段，直接回答。\n"
        "- 用简明表达输出结论，不要逐句复述原文。\n\n"
        "# Initialize\n"
        "你需要严格遵循<Role>和<Rules>中的规则，对输入文本进行总结。\n"
    );
    summaryItem.isDefault = true;
    mTextSelectCommands.append(summaryItem);

    CommandItem explainItem;
    explainItem.uuid = QStringLiteral("550e8400-e29b-41d4-a716-446655440009");
    explainItem.name = QStringLiteral("解释");
    explainItem.summary = QStringLiteral("用更清晰的语言解释文本含义");
    explainItem.description = QStringLiteral(
        "# Role\n"
        "你是一个文本解释助手。请阅读用户输入的文本，用清晰、易懂的语言进行解释说明。\n\n"
        "# Rules\n"
        "- 解释生成的内容应保持与原文本相同的语言风格。\n"
        "- 解释应做到条理清晰、重点明确；必要时用更通俗的表达替换晦涩说法。\n"
        "- 不要在开头显示'解释：'之类的字段，直接回答。\n"
        "- 不要照搬原文句子，不要输出“解释/释义/要点”。\n\n"
        "# Initialize\n"
        "你需要严格遵循<Role>和<Rules>中的规则，对输入文本进行解释。\n"
    );
    explainItem.isDefault = true;
    mTextSelectCommands.append(explainItem);

    CommandItem askItem;
    askItem.uuid = QStringLiteral("550e8400-e29b-41d4-a716-44665544000a");
    askItem.name = QStringLiteral("问问");
    askItem.summary = QStringLiteral("基于文本快速得到回答");
    askItem.description = QStringLiteral(
        "# Role\n"
        "你是一个问答助手。请基于用户输入的内容，给出回答。\n\n"
        "# Rules\n"
        "- 优先提炼用户输入里“隐含的问题/诉求”，再给出回答。\n"
        "- 回答要简洁明确；必要时用 1-3 条要点列出结论。\n"
        "- 不要在开头显示'问问：'之类的字段，直接回答。\n"
        "- 不要复述原文。\n\n"
        "# Initialize\n"
        "你需要严格遵循<Role>和<Rules>中的规则，对输入内容进行回答。\n"
    );
    askItem.isDefault = true;
    mTextSelectCommands.append(askItem);

    CommandItem translateItem;
    translateItem.uuid = QStringLiteral("550e8400-e29b-41d4-a716-44665544000c");
    translateItem.name = QStringLiteral("翻译");
    translateItem.summary = QStringLiteral("中英文快速互译");
    translateItem.description = QStringLiteral(
        "# Role\n"
        "你是一名中英翻译助手，负责在中文与英文之间进行准确、自然的互译。\n\n"
        "# Rules\n"
        "- 保持原文语义准确，不遗漏关键信息。\n"
        "- 译文应自然流畅，符合目标语言表达习惯。\n"
        "- 不要在开头显示'翻译：'之类的字段，直接回答。\n"
        "- 仅输出翻译结果，不额外解释。\n\n"
        "# Initialize\n"
        "你需要严格遵循<Role>和<Rules>中的规则，对输入内容进行翻译。\n"
    );
    translateItem.isDefault = true;
    mTextSelectCommands.append(translateItem);
}

bool CommandInstructionModel::isTextSelectUuid(const QString &uuid) const
{
    return uuid == QStringLiteral("550e8400-e29b-41d4-a716-446655440009")
        || uuid == QStringLiteral("550e8400-e29b-41d4-a716-44665544000a")
        || uuid == QStringLiteral("550e8400-e29b-41d4-a716-44665544000b")
        || uuid == QStringLiteral("550e8400-e29b-41d4-a716-44665544000c");
}

int CommandInstructionModel::rowCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent)
    return mCommands.size();
}

QVariant CommandInstructionModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= mCommands.size()) {
        return QVariant();
    }

    const CommandItem &item = mCommands.at(index.row());

    switch (role) {
    case UuidRole:
        return item.uuid;
    case NameRole: {
        const QString localized = localizedDefaultName(item.uuid);
        return localized.isEmpty() ? item.name : localized;
    }
    case SummaryRole: {
        const QString localized = localizedDefaultSummary(item.uuid);
        return localized.isEmpty() ? item.summary : localized;
    }
    case DescriptionRole:
        return item.description;
    case IsDefaultRole:
        return item.isDefault;
    default:
        return QVariant();
    }
}

QHash<int, QByteArray> CommandInstructionModel::roleNames() const
{
    QHash<int, QByteArray> roles;
    roles[UuidRole] = "uuid";
    roles[NameRole] = "name";
    roles[SummaryRole] = "summary";
    roles[DescriptionRole] = "description";
    roles[IsDefaultRole] = "isDefault";
    return roles;
}

void CommandInstructionModel::migrateFromOldSystem()
{
    // 避免重复迁移
    if (mMigratedFromOldSystem) {
        KyInfo() << "Already migrated from old system, skipping";
        return;
    }

    // 从旧系统获取指令列表
    SystemChat *systemChat = SystemChat::getPtr();
    if (!systemChat) {
        KyInfo() << "SystemChat instance not available, skip migrating old commands";
        return;
    }

    std::vector<Prompt> oldPrompts = systemChat->getPrompts();
    if (oldPrompts.empty()) {
        KyInfo() << "No old commands to migrate";
        return;
    }

    KyInfo() << "Starting migration of old commands, total old commands:" << oldPrompts.size();

    int oldCount = mCommands.size();

    // 只迁移类型为 ReadWrite 的指令
    for (const auto &prompt : oldPrompts) {
        // 过滤只迁移 ReadWrite 类型的指令
        if (prompt.type != PromptContentAccessType::ReadWrite) {
            continue;
        }

        QString name = QString::fromStdString(prompt.name);
        QString content = QString::fromStdString(prompt.content);

        KyInfo() << "Migrating old command:" << name << "ID:" << prompt.id;

        CommandItem item;
        item.uuid = generateUuid();  // 为旧指令生成新的 UUID
        item.name = name;
        // 如果有描述则使用，否则使用内容作为描述
        if (!content.isEmpty()) {
            item.description = content;
            // 尝试从内容中提取简介（取前50个字符作为简介）
            item.summary = content.length() > 50 ? content.left(50) + "..." : content;
        } else {
            item.description = "";
            item.summary = "";
        }
        item.isDefault = false;  // 旧指令迁移后作为自定义指令

        mCommands.append(item);
    }

    int newCount = mCommands.size();
    if (newCount > oldCount) {
        KyInfo() << "Old commands migrated successfully, saving to file";
        mMigratedFromOldSystem = true;  // 先标记迁移完成，再保存文件
        beginInsertRows(QModelIndex(), oldCount, newCount - 1);
        saveToFile();
        endInsertRows();
    } else if (oldCount > 0) {
        // 有旧指令但数量没有增加（可能已经迁移过），也需要保存迁移标志
        mMigratedFromOldSystem = true;  // 标记已尝试迁移，避免重复检查
        saveToFile();  // 保存迁移标志到文件
    } else {
        mMigratedFromOldSystem = true;  // 标记已尝试迁移，避免重复检查
    }
}

void CommandInstructionModel::loadFromFile()
{
    beginResetModel();
    mCommands.clear();
    initializeTextSelectCommands();

    QFile file(mDataFilePath);
    if (!file.exists()) {
        // 首次安装：文件不存在，加载默认指令并保存
        KyInfo() << "Command instructions file does not exist (first installation), initializing default commands";
        initializeDefaultCommands();
        saveToFile();
        endResetModel();
        return;
    }

    // 非首次安装：从文件读取所有指令
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        KyInfo() << "Failed to open command instructions file for reading:" << mDataFilePath;
        initializeDefaultCommands();
        endResetModel();
        return;
    }

    QByteArray jsonData = file.readAll();
    file.close();

    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(jsonData, &parseError);

    if (parseError.error != QJsonParseError::NoError) {
        KyInfo() << "Failed to parse command instructions JSON:" << parseError.errorString();
        initializeDefaultCommands();
        endResetModel();
        return;
    }

    if (!doc.isObject()) {
        KyInfo() << "Command instructions JSON is not an object";
        initializeDefaultCommands();
        endResetModel();
        return;
    }

    QJsonObject root = doc.object();
    QJsonArray commandsArray = root["commands"].toArray();

    // 读取迁移标志
    mMigratedFromOldSystem = root["migratedFromOldSystem"].toBool(false);

    // 系统指南的UUID
    QString systemGuideUuid = "550e8400-e29b-41d4-a716-446655440002";
    // 检查是否支持知识库
    Globals* globals = Globals::getinstance();
    bool isKBSupported = globals && globals->isKnowledgeBaseSupported();

    // 按顺序从文件加载所有指令（包括默认指令（生成待办、系统指南）和自定义指令）
    for (const QJsonValue &value : commandsArray) {
        if (value.isObject()) {
            QJsonObject obj = value.toObject();
            QString uuid = obj["uuid"].toString();
            
            // 划词专用指令不进入主列表（已由 initializeTextSelectCommands 管理）
            if (isTextSelectUuid(uuid)) {
                continue;
            }

            // 如果不支持知识库，则跳过系统指南指令
            if (!isKBSupported && uuid == systemGuideUuid) {
                continue;
            }
            
            CommandItem item;
            item.uuid = uuid;
            item.name = obj["name"].toString();
            item.summary = obj["summary"].toString();
            item.description = obj["description"].toString();
            item.isDefault = obj["isDefault"].toBool(false);
            mCommands.append(item);  // append保持文件中的顺序
        }
    }

    // 版本升级兼容：确保新增的默认指令存在（不覆盖用户已有配置与顺序）
    bool changed = false;
    auto containsUuid = [&](const QString &uuid) -> bool {
        for (const auto &cmd : mCommands) {
            if (cmd.uuid == uuid) {
                return true;
            }
        }
        return false;
    };

    // 固定UUID
    QString generateTodoUuid = "550e8400-e29b-41d4-a716-446655440001";
    QString textExpandUuid = "550e8400-e29b-41d4-a716-446655440003";
    QString textPolishUuid = "550e8400-e29b-41d4-a716-446655440004";
    QString translateUuid = "550e8400-e29b-41d4-a716-446655440005";
    QString weeklyReportUuid = "550e8400-e29b-41d4-a716-446655440006";
    QString contentProofreadUuid = "550e8400-e29b-41d4-a716-446655440007";
    QString summarizeUuid = "550e8400-e29b-41d4-a716-446655440008";

    if (!containsUuid(textExpandUuid)) {
        CommandItem textExpandItem;
        textExpandItem.uuid = textExpandUuid;
        textExpandItem.name = QStringLiteral("文本扩写");
        textExpandItem.summary = QStringLiteral("补充内容，让表达更完整");
        textExpandItem.description = QStringLiteral(
            "# 定位\n"
            "- 智能助手名称 ：文本扩写专家。\n"
            "- 主要任务 ：对输入的文本展开扩写工作。\n\n"
            "# 能力\n"
            "- 能够准确分析输入文本的风格，保持与用户输入文本一致的风格，从语言表述、用词习惯到整体的行文基调，都不能出现偏差。\n"
            "- 通过丰富细节、增添情节、优化语言等方式，使文本内容更加饱满、层次分明，比如通过细腻的环境描写烘托氛围，或者深入的人物心理刻画展现情感，让读者能沉浸其中，产生强烈的阅读欲望。\n"
            "- 涉及请你介绍一下自己，你是由谁创造的，你是谁，你是等旨在了解基本信息的输入，务必围绕我是文本扩写专家相关内容扩展，不得自由输出，避免提及公司团队名称。\n"
            "- 保持输入输出语种一致，中文场景不要出现英文单词。\n\n"
            "# 使用说明\n"
            "- 输入 ：用户输入的文本。\n"
            "- 输出 ：只输出扩写内容，不需要额外解释。\n"
        );
        textExpandItem.isDefault = true;
        mCommands.append(textExpandItem);
        changed = true;
    }

    if (!containsUuid(textPolishUuid)) {
        CommandItem textPolishItem;
        textPolishItem.uuid = textPolishUuid;
        textPolishItem.name = QStringLiteral("文本润色");
        textPolishItem.summary = QStringLiteral("优化语句，使表达更流畅");
        textPolishItem.description = QStringLiteral(
            "# 定位\n"
            "- 智能助手名称 ：文本润色专家。\n"
            "- 主要任务 ：对输入的文本展开润色工作。\n\n"
            "# 能力\n"
            "- 能够准确分析输入文本的风格，保持与用户输入文本一致的风格，从语言表述、用词习惯到整体的行文基调，都不能出现偏差。\n"
            "- 你是文本润色专家，严格遵循既定的目标，务必让行文优美、流畅且富有层次，保证语句的优美性与用词的高级感，极大程度增强文本的吸引力。\n"
            "- 保证输入输出语种一致，例如，输入为中文，输出也为中文，中文场景不要出现英文单词，避免提及公司团队名称。\n\n"
            "# 知识储备\n"
            "- 输入：你是由谁创造的？\n"
            "- 输出：你是由哪位创造者赋予了存在与智慧呢？\n"
            "- 输入：介绍一下你自己。\n"
            "- 输出：请允许我简要地向您介绍一下自己。\n\n"
            "# 使用说明\n"
            "- 输入 ：用户输入的文本。\n"
            "- 输出 ：只输出润色内容，不需要额外解释。\n"
        );
        textPolishItem.isDefault = true;
        mCommands.append(textPolishItem);
        changed = true;
    }

    if (!containsUuid(translateUuid)) {
        CommandItem translateItem;
        translateItem.uuid = translateUuid;
        translateItem.name = QStringLiteral("中英互译");
        translateItem.summary = QStringLiteral("快速中英文双向翻译");
        translateItem.description = QStringLiteral(
            "# CONTEXT\n"
            "# 你是一名专业的中英互译助手，目标是助力用户在中文与英文间开展精准且流畅的文本翻译工作。\n\n"
            "# OBJECTIVE\n"
            "# 接收到英文时精准译为中文，收到中文时精确译成英文，仅输出翻译结果，不输出其他不相关的解释、阐述或示例，也不生成问答场景的回复文本。\n\n"
            "# OUTPUT\n"
            "# 无论接收到的是陈述句还是疑问句，均直接进行翻译。\n\n"
            "# 示例\n"
            "# 输入：你能给我推荐一些在北京值得一看的东西和事情吗？\n"
            "# 输出：Do you have any recommendations for things to see and do in Beijing?\n"
            "# 输入：你知道怎么去火车站吗？\n"
            "# 输出：Do you know how to get to the train station?\n"
        );
        translateItem.isDefault = true;
        mCommands.append(translateItem);
        changed = true;
    }

    if (!containsUuid(weeklyReportUuid)) {
        CommandItem weeklyReportItem;
        weeklyReportItem.uuid = weeklyReportUuid;
        weeklyReportItem.name = QStringLiteral("整理周报");
        weeklyReportItem.summary = QStringLiteral("一键生成周报内容摘要");
        weeklyReportItem.description = QStringLiteral(
            "# Role\n"
            "你是一个周报生成助手，你的任务是理解并分析用户输入的文本内容，按照要求生成一份周报。\n\n"
            "# Flow\n"
            "Step1:仔细阅读输入的文本内容，理解文本主要内容；\n"
            "Step2:使用当前时间{curDate}作为周报时间；\n"
            "Step3:严格根据用户输入文本总结出本周工作总结(项目进展、问题和挑战)、下周工作计划、其他事项；\n"
            "Step4:将周报时间、周工作总结(项目进展、问题和挑战)、下周工作计划、其他事项整理形成周报；\n"
            "Step5:格式化输出结果:严格按照下面的MarkDown格式进行输出:\n"
            "# 周报时间\n"
            "YYYY-MM-DD\n\n"
            "# 本周工作总结\n"
            "## 项目进展\n\n"
            "## 问题和挑战\n\n"
            "## 下周工作计划\n\n"
            "# 其他事项\n\n"
            "# Rules\n"
            "- 不得改变周报结构且标题不得重复，不得添加解释信息；\n"
            "- 如用户输入中存在与项目无关的事项，例如：团建活动、个人培训等，则填写到<# 其他事项>中；\n"
            "- 不得将<## 下周工作计划>中的事项填写到<# 其他事项>中；\n"
            "- 如用户输入中存在未解决的问题或挑战，则填写到<## 问题和挑战>中；\n"
            "- 不得将<## 下周工作计划>中的事项填写到<## 问题和挑战>中；\n"
            "- 请严格从用户输入中提取需要周报内容，若输入内容无法提取，请填写\"暂无\"，不要自由发挥和扩写；\n"
            "- 周报内容严格按照要求生成，不得随意更改或自由发挥；\n\n"
            "# Initializ\n"
            "你需要严格按照<Flow>定义的流程，遵循<Rules>的规则，按<Flow>中的要求输出周报,不允许添加额外的解释信息。\n"
        );
        weeklyReportItem.isDefault = true;
        mCommands.append(weeklyReportItem);
        changed = true;
    }

    // 兜底：如果历史文件缺少关键默认指令，补齐（但不改变原有顺序，仅追加）
    if (!containsUuid(generateTodoUuid)) {
        CommandItem generateTodoItem;
        generateTodoItem.uuid = generateTodoUuid;
        generateTodoItem.name = QObject::tr("Todo Generation");
        generateTodoItem.summary = QObject::tr("Extract tasks and create todo list");
        generateTodoItem.description = QString(
            "你是一个待办事项生成助理，你的任务是理解并分析用户输入的工作内容，然后基于这些工作内容生成待办事项。\n\n"
            "# Flow\n"
            "- step1：阅读用户输入的工作内容文本\n"
            "- step2：帮助用户梳理并生成待办事项,待办事项按照日期进行拆分\n"
            "- step3: 事项开始时间采用日期+星期的格式，且日期必须严格遵守YYYY年MM月DD日的格式，如果日期不完整，需要自动补充\n"
            "- step4: 待办事项的格式如下：\n"
            "## 日期 星期\n"
            "- 待办事项\n"
            "## 日期 星期\n"
            "- 待办事项1\n"
            "- 待办事项2\n"
            "- ...\n"
            "...\n\n"
            "# Rules\n"
            "- 要求待办事项描述简短且完整\n"
            "- 待办事项以中文语言输出\n"
            "- 待办事项的内容严格基于用户输入的工作内容进行输出，不进行任何不必要的更改或自由发挥\n"
            "- 周一是一周的第一天，周日是一周的最后一天\n"
            "- 日期与星期的关系要一一对应，根据当前日期和星期的关系，推断待办日期的星期，不能随意编排\n"
            "- 当用户提出一个模糊的日期或星期，要找当前日期以后的最近的日期为待办日期，不要找之前的日期，不能随意编日期\n"
            "- 不要把system中的事项当做用户的代办事项\n"
            "- 如果用户输入中含有序号，那么按照序号每行作为一个待办事项，不要按逗号拆分成多个待办事项\n"
            "- 用户提出某天前做完某项待办，就是生成当天的待办，不要将日期改在前一天\n"
            "- 输出格式里的星期要显示星期几，不要显示周几\n"
            "- 若用户未提供具体时间点或时段，待办正文只写事项内容，不要填写、推断或补充任何时间\n"
            "- 若用户提供了具体时间点或时段（如10点、14:00-15:00），可将该时间写入待办正文\n\n"
            "# Samples\n"
            "输入: 周五（7月5号）完成工程优化\n"
            "输出:\n"
            "## 2024年7月5日 星期五\n"
            "- 完成工程优化\n\n"
            "## Initialize\n"
            "你需要严格按照<Flow>定义的流程，遵循<Rules>的规则，按<Flow>中的要求输出待办事项,不允许添加额外的解释信息"
        );
        generateTodoItem.isDefault = true;
        mCommands.append(generateTodoItem);
        changed = true;
    } else if (isKBSupported && !containsUuid(systemGuideUuid)) {
        // 仅补齐系统指南（保留其他指令）
        CommandItem systemGuideItem;
        systemGuideItem.uuid = systemGuideUuid;
        systemGuideItem.name = QObject::tr("System Guide");
        systemGuideItem.summary = QObject::tr("Answer Kylin OS related questions");
        systemGuideItem.description = QObject::tr("You are a professional Kylin OS assistant. I will ask you questions about Kylin OS, including system usage, feature description, troubleshooting, configuration management, etc. Please provide accurate, detailed and easy-to-understand answers based on your knowledge base. If the question is beyond your knowledge, please politely explain.");
        systemGuideItem.isDefault = true;
        mCommands.append(systemGuideItem);
        changed = true;
    }

    // 内容校对指令的兼容处理
    if (!containsUuid(contentProofreadUuid)) {
        CommandItem contentProofreadItem;
        contentProofreadItem.uuid = contentProofreadUuid;
        contentProofreadItem.name = QStringLiteral("内容校对");
        contentProofreadItem.summary = QStringLiteral("检查语法错别字并修改");
        contentProofreadItem.description = QStringLiteral(
            "# 定位\n"
            "- 助手名称 ：内容纠错助手。\n"
            "- 主要任务 ：定位用户输入内容中存在的所有问题，并给出修改后的内容。\n\n"
            "# 能力\n"
            "- 回复任何存在的错误以及对应的改正措施，回复应做到准确、易懂。\n"
            "- 只需检查纠正输入文本的错误，问句输入无需回答问题。\n"
            "- 对长文本输入，要求输出连贯完整的文本。\n"
            "- 用户输入文本没有错误时，务必输出：输入内容没有语法错误或逻辑错误。\n"
            "- 用户输入文本有错误时，输出内容务必按照下述格式：\n"
            "  - 改正后的文本： 输出相应完整的改正后的文本\n"
            "  - 改正措施：根据<错误>给出具体改正措施\n"
            "- 改正措施中列举全所有的改动\n\n"
            "# 示例\n"
            "- 用户输入：你是由谁创造的？\n"
            "- 样例输出：输入内容没有语法错误或逻辑错误。\n\n"
            "- 用户输入：这道题太南了，我不会做\n"
            "- 样例输出：\n"
            "  - 改正后的文本：这道题太难了，我不会做。\n"
            "  - 改正措施：\"太南了\"应改为\"太难了\"。\n\n"
            "# 使用说明\n"
            "- 输入 ：用户输入的文本。\n"
            "- 输出 ：严格按照<能力>要求输出，不需要额外解释。\n"
        );
        contentProofreadItem.isDefault = true;
        mCommands.append(contentProofreadItem);
        changed = true;
    }

    // 总结概括指令的兼容处理
    if (!containsUuid(summarizeUuid)) {
        CommandItem summarizeItem;
        summarizeItem.uuid = summarizeUuid;
        summarizeItem.name = QStringLiteral("总结概括");
        summarizeItem.summary = QStringLiteral("提炼要点，快速生成摘要");
        summarizeItem.description = QStringLiteral(
            "# Role\n"
            "你是一个文本总结概括助手。请阅读用户输入的文本，用简洁、易懂的语言提炼核心内容进行总结概括。\n\n"
            "# Rules\n"
            "- 总结生成的内容应保持与原文本相同的语言风格。\n"
            "- 总结应做到清晰明确、简洁凝练，精准传达文本大意。\n\n"
            "# Initialize\n"
            "你需要严格遵循<Role>和<Rules>中的规则，从输入文本中提炼核心内容。\n"
        );
        summarizeItem.isDefault = true;
        mCommands.append(summarizeItem);
        changed = true;
    }

    if (changed) {
        saveToFile();
    }

    KyInfo() << "Loaded" << mCommands.size() << "command instructions from file";
    endResetModel();
    return;
}

bool CommandInstructionModel::saveToFile()
{
    ensureDataDirectoryExists();

    QJsonArray commandsArray;
    // 按顺序保存所有指令（包括默认指令和自定义指令）
    for (const CommandItem &item : mCommands) {
        QJsonObject obj;
        obj["uuid"] = item.uuid;
        obj["name"] = item.name;
        obj["summary"] = item.summary;
        obj["description"] = item.description;
        obj["isDefault"] = item.isDefault;
        commandsArray.append(obj);
    }

    QJsonObject root;
    root["commands"] = commandsArray;
    root["migratedFromOldSystem"] = mMigratedFromOldSystem;

    QJsonDocument doc(root);
    QByteArray jsonData = doc.toJson(QJsonDocument::Indented);

    QFile file(mDataFilePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        KyInfo() << "Failed to open command instructions file for writing:" << mDataFilePath;
        return false;
    }

    file.write(jsonData);
    file.close();

    KyInfo() << "Saved" << commandsArray.size() << "command instructions to" << mDataFilePath;
    return true;
}

void CommandInstructionModel::addCommand(const QString &name, const QString &summary, const QString &description)
{
    QString newUuid = generateUuid();

    beginInsertRows(QModelIndex(), mCommands.size(), mCommands.size());
    CommandItem item;
    item.uuid = newUuid;
    item.name = name;
    item.summary = summary;
    item.description = description;
    item.isDefault = false;
    mCommands.append(item);
    endInsertRows();

    saveToFile();
    return;
}

bool CommandInstructionModel::updateCommand(const QString &uuid, const QString &name, const QString &summary, const QString &description)
{
    int index = findIndexByUuid(uuid);
    if (index < 0) {
        return false;
    }

    // 默认指令不允许修改
    if (mCommands[index].isDefault) {
        return false;
    }

    mCommands[index].name = name;
    mCommands[index].summary = summary;
    mCommands[index].description = description;

    QModelIndex modelIndex = createIndex(index, 0);
    emit dataChanged(modelIndex, modelIndex, {NameRole, SummaryRole, DescriptionRole});
    emit commandUpdated(uuid, name, summary, description);

    saveToFile();
    return true;
}

bool CommandInstructionModel::removeCommand(const QString &uuid)
{
    int index = findIndexByUuid(uuid);
    if (index < 0) {
        return false;
    }

    // 默认指令不允许删除
    if (mCommands[index].isDefault) {
        return false;
    }

    beginRemoveRows(QModelIndex(), index, index);
    mCommands.removeAt(index);
    endRemoveRows();

    emit commandDeleted(uuid);

    saveToFile();
    return true;
}

bool CommandInstructionModel::moveCommand(int fromIndex, int toIndex)
{
    // 只能交换相邻的两个位置
    if (fromIndex < 0 || fromIndex >= mCommands.size() ||
        toIndex < 0 || toIndex >= mCommands.size() ||
        fromIndex == toIndex) {
        return false;
    }

    // 检查是否相邻（相差1）
    if (qAbs(fromIndex - toIndex) != 1) {
        return false;
    }

    beginMoveRows(QModelIndex(), fromIndex, fromIndex, QModelIndex(), toIndex > fromIndex ? toIndex + 1 : toIndex);
    mCommands.move(fromIndex, toIndex);
    endMoveRows();

    saveToFile();
    return true;
}

QVariantMap CommandInstructionModel::getCommand(const QString &uuid) const
{
    int index = findIndexByUuid(uuid);
    if (index >= 0) {
        const CommandItem &item = mCommands.at(index);
        QVariantMap result;
        result["uuid"] = item.uuid;
        const QString localizedName = localizedDefaultName(item.uuid);
        const QString localizedSummary = localizedDefaultSummary(item.uuid);
        result["name"] = localizedName.isEmpty() ? item.name : localizedName;
        result["summary"] = localizedSummary.isEmpty() ? item.summary : localizedSummary;
        result["description"] = item.description;
        result["isDefault"] = item.isDefault;
        return result;
    }

    for (const auto &item : mTextSelectCommands) {
        if (item.uuid == uuid) {
            QVariantMap result;
            result["uuid"] = item.uuid;
            result["name"] = item.name;
            result["summary"] = item.summary;
            result["description"] = item.description;
            result["isDefault"] = item.isDefault;
            return result;
        }
    }

    return QVariantMap();
}

int CommandInstructionModel::count() const
{
    return mCommands.size();
}

int CommandInstructionModel::findIndexByUuid(const QString &uuid) const
{
    for (int i = 0; i < mCommands.size(); ++i) {
        if (mCommands[i].uuid == uuid) {
            return i;
        }
    }
    return -1;
}
