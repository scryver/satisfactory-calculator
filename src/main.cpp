#if _MSC_VER
#define NO_INTRINSICS 1
#include <intrin.h>
#else
#include <x86intrin.h>
#endif

#include "../libberdip/src/common.h"
#include "../libberdip/src/maps.h"
#include "../libberdip/src/maths.h"
#include "../libberdip/src/strings.h"
#include "../libberdip/src/files.h"

global const char *gSpaces = "                                                                                                               ";

struct Item
{
    String name;
    f32 itemsPerMinute;
};

enum Building
{
    NoBuilding,
    Miner,
    OilExtractor,
    Smelter,
    Foundry,
    Constructor,
    Assembler,
    Manufacturer,
    Refinery,
    Packager,
    Blender,

    BuildingCount
};

global f32 gPowerForBuilding[BuildingCount] =
{
    0.0f,
    12.0f, // NOTE(NAME): Miner Mk2 only
    40.0f, // OilExtractor
    4.0f,  // Smelter
    16.0f, // Foundry
    4.0f,  // Constructor
    15.0f, // Assembler
    55.0f, // Manufacturer
    30.0f, // Refinery
};

struct Recipe
{
    Building building;
    u32 inputCount;
    Item inputs[4];
    Item output;
    // TODO(michiel): Handle residual output
    Item extraOutput;
};

struct Calculator
{
    Interns strings;

    u32 maxRecipeCount;
    u32 recipeCount;
    Recipe *recipes;
};

struct CostTest
{
    u32 consumeCount;
    Item consumedItems[128];
    u32 produceCount;
    Item producedItems[128];
    f32 buildingCounts[BuildingCount];
};

internal String
string_from_building(Building building, b32 single = true)
{
    String result = {};
    switch (building)
    {
        case Miner: {
            if (single) {
                result = static_string("miner");
            } else {
                result = static_string("miners");
            }
        } break;

        case OilExtractor: {
            if (single) {
                result = static_string("oil extractor");
            } else {
                result = static_string("oil extractors");
            }
        } break;

        case Smelter: {
            if (single) {
                result = static_string("smelter");
            } else {
                result = static_string("smelters");
            }
        } break;

        case Foundry: {
            if (single) {
                result = static_string("foundry");
            } else {
                result = static_string("foundries");
            }
        } break;

        case Constructor: {
            if (single) {
                result = static_string("constructor");
            } else {
                result = static_string("constructors");
            }
        } break;

        case Assembler: {
            if (single) {
                result = static_string("assembler");
            } else {
                result = static_string("assemblers");
            }
        } break;

        case Manufacturer: {
            if (single) {
                result = static_string("manufacturer");
            } else {
                result = static_string("manufacturers");
            };
        } break;

        case Refinery: {
            if (single) {
                result = static_string("refinery");
            } else {
                result = static_string("refineries");
            }
        } break;

        case Packager: {
            if (single) {
                result = static_string("packager");
            } else {
                result = static_string("packagers");
            }
        } break;

        case Blender: {
            if (single) {
                result = static_string("blender");
            } else {
                result = static_string("blenders");
            }
        } break;

        INVALID_DEFAULT_CASE;
    };

    return result;
}

internal Item *
get_consume_item(CostTest *cost, String name)
{
    Item *result = 0;
    for (u32 consumeIdx = 0; consumeIdx < cost->consumeCount; ++consumeIdx)
    {
        Item *test = cost->consumedItems + consumeIdx;
        if (test->name == name)
        {
            result = test;
            break;
        }
    }
    return result;
}

internal Item *
get_produce_item(CostTest *cost, String name)
{
    Item *result = 0;
    for (u32 produceIdx = 0; produceIdx < cost->produceCount; ++produceIdx)
    {
        Item *test = cost->producedItems + produceIdx;
        if (test->name == name)
        {
            result = test;
            break;
        }
    }
    return result;
}

internal void
add_recipe_cost(CostTest *cost, Recipe *recipe, f32 ratio)
{
    Item *produced = get_produce_item(cost, recipe->output.name);

    if (!produced)
    {
        i_expect(cost->produceCount < array_count(cost->producedItems));
        produced = cost->producedItems + cost->produceCount++;
        produced->name = recipe->output.name;
        produced->itemsPerMinute = 0;
    }
    produced->itemsPerMinute += ratio * recipe->output.itemsPerMinute;
    cost->buildingCounts[recipe->building] += ratio;

    if (recipe->extraOutput.name.size)
    {
        Item *extra = get_produce_item(cost, recipe->extraOutput.name);

        if (!extra)
        {
            i_expect(cost->produceCount < array_count(cost->producedItems));
            extra = cost->producedItems + cost->produceCount++;
            extra->name = recipe->extraOutput.name;
            extra->itemsPerMinute = 0;
        }
        extra->itemsPerMinute += recipe->extraOutput.itemsPerMinute * ratio;
    }

    for (u32 inputIdx = 0; inputIdx < recipe->inputCount; ++inputIdx)
    {
        Item *input = recipe->inputs + inputIdx;
        Item *consumed = get_consume_item(cost, input->name);

        if (!consumed)
        {
            i_expect(cost->consumeCount < array_count(cost->consumedItems));
            consumed = cost->consumedItems + cost->consumeCount++;
            consumed->name = input->name;
            consumed->itemsPerMinute = 0;
        }
        consumed->itemsPerMinute += input->itemsPerMinute * ratio;
    }
}

internal void
output_input_cost(CostTest *cost)
{
    for (s32 consumeIdx = cost->consumeCount - 1; consumeIdx >= 0; )
    {
        Item *consumer = cost->consumedItems + consumeIdx;
        Item *producer = get_produce_item(cost, consumer->name);
        if (producer)
        {
            if (producer->itemsPerMinute > consumer->itemsPerMinute)
            {
                producer->itemsPerMinute -= consumer->itemsPerMinute;
                cost->consumedItems[consumeIdx] = cost->consumedItems[--cost->consumeCount];
            }
            else if (producer->itemsPerMinute < consumer->itemsPerMinute)
            {
                consumer->itemsPerMinute -= producer->itemsPerMinute;
                *producer = cost->producedItems[--cost->produceCount];
                --consumeIdx;
            }
            else
            {
                cost->consumedItems[consumeIdx] = cost->consumedItems[--cost->consumeCount];
                *producer = cost->producedItems[--cost->produceCount];
            }
        }
        else
        {
            --consumeIdx;
        }
    }
}

internal void
print_cost(CostTest *cost)
{
    fprintf(stdout, "Consumed:\n");
    for (u32 consumeIdx = 0; consumeIdx < cost->consumeCount; ++consumeIdx)
    {
        Item *item = cost->consumedItems + consumeIdx;
        fprintf(stdout, "  %.*s : %5.2f / minute\n", STR_FMT(item->name), item->itemsPerMinute);
    }

    fprintf(stdout, "Produced:\n");
    for (u32 produceIdx = 0; produceIdx < cost->produceCount; ++produceIdx)
    {
        Item *item = cost->producedItems + produceIdx;
        fprintf(stdout, "  %.*s : %5.2f / minute\n", STR_FMT(item->name), item->itemsPerMinute);
    }

    fprintf(stdout, "Buildings:\n");
    i_expect(array_count(cost->buildingCounts) <= BuildingCount);
    f32 totalPower = 0.0f;
    for (u32 idx = 1; idx < array_count(cost->buildingCounts); ++idx)
    {
        f32 value = cost->buildingCounts[idx];
        if (value != 0.0f)
        {
            String name = string_from_building((Building)idx, value == 1.0f);
            f32 powerUsage = value * gPowerForBuilding[idx];
            fprintf(stdout, "  %.*s : %5.2fx, %5.1f MW\n", STR_FMT(name), value, powerUsage);
            totalPower += powerUsage;
        }
    }
    fprintf(stdout, "Total power usage: %5.1fMW\n", totalPower);
}

internal String
add_item(Calculator *calculator, String name)
{
    String result = str_intern(&calculator->strings, name);
    return result;
}

internal void
add_extra_item(Calculator *calculator, Recipe *recipe, String name, f32 itemsPerMinute)
{
    recipe->extraOutput.name = add_item(calculator, name);
    recipe->extraOutput.itemsPerMinute = itemsPerMinute;
}

internal Recipe *
add_recipe(Calculator *calculator, Building building, String outputName, f32 outputPerMinute, String inputName, f32 inputPerMinute)
{
    i_expect(calculator->recipeCount < calculator->maxRecipeCount);
    Recipe *result = calculator->recipes + calculator->recipeCount++;

    result->building = building;
    result->inputCount = 1;
    result->inputs[0].name = add_item(calculator, inputName);
    result->inputs[0].itemsPerMinute = inputPerMinute;
    result->output.name = add_item(calculator, outputName);
    result->output.itemsPerMinute = outputPerMinute;

    return result;
}

internal Recipe *
add_recipe(Calculator *calculator, Building building, String outputName, f32 outputPerMinute, String inputName1, f32 inputPerMinute1, String inputName2, f32 inputPerMinute2)
{
    Recipe *result = add_recipe(calculator, building, outputName, outputPerMinute, inputName1, inputPerMinute1);

    result->inputCount = 2;
    result->inputs[1].name = add_item(calculator, inputName2);
    result->inputs[1].itemsPerMinute = inputPerMinute2;

    return result;
}

internal Recipe *
add_recipe(Calculator *calculator, Building building, String outputName, f32 outputPerMinute, String inputName1, f32 inputPerMinute1, String inputName2, f32 inputPerMinute2, String inputName3, f32 inputPerMinute3)
{
    Recipe *result = add_recipe(calculator, building, outputName, outputPerMinute, inputName1, inputPerMinute1, inputName2, inputPerMinute2);

    result->inputCount = 3;
    result->inputs[2].name = add_item(calculator, inputName3);
    result->inputs[2].itemsPerMinute = inputPerMinute3;

    return result;
}

internal Recipe *
add_recipe(Calculator *calculator, Building building, String outputName, f32 outputPerMinute, String inputName1, f32 inputPerMinute1, String inputName2, f32 inputPerMinute2, String inputName3, f32 inputPerMinute3, String inputName4, f32 inputPerMinute4)
{
    Recipe *result = add_recipe(calculator, building, outputName, outputPerMinute, inputName1, inputPerMinute1, inputName2, inputPerMinute2, inputName3, inputPerMinute3);

    result->inputCount = 4;
    result->inputs[3].name = add_item(calculator, inputName4);
    result->inputs[3].itemsPerMinute = inputPerMinute4;

    return result;
}

internal u32
get_recipe_count(Calculator *calculator, String name)
{
    u32 result = 0;
    name = str_intern(&calculator->strings, name);
    for (u32 recipeIdx = 0; recipeIdx < calculator->recipeCount; ++recipeIdx)
    {
        Recipe *test = calculator->recipes + recipeIdx;
        if (test->output.name == name)
        {
            ++result;
        }
    }

    return result;
}

internal Recipe *
get_recipe(Calculator *calculator, String name, u32 skip = 0)
{
    Recipe *result = 0;
    name = str_intern(&calculator->strings, name);
    for (u32 recipeIdx = 0; recipeIdx < calculator->recipeCount; ++recipeIdx)
    {
        Recipe *test = calculator->recipes + recipeIdx;
        if (test->output.name == name)
        {
            if (skip) {
                --skip;
            } else {
                result = test;
                break;
            }
        }
    }

    return result;
}

internal void
print_line(FileStream output, const char *fmt, ...)
{
    fprintf(stream2stdfile(output), "%.*s", output.indent * 2, gSpaces);

    va_list args;
    va_start(args, fmt);
    vfprintf(stream2stdfile(output), fmt, args);
    va_end(args);

    fprintf(stream2stdfile(output), "\n");
}

internal void
calc_total_production(Calculator *calculator, CostTest *cost, Recipe *recipe, f32 expectedPerMinute)
{
    f32 ratio = expectedPerMinute / recipe->output.itemsPerMinute;
    add_recipe_cost(cost, recipe, ratio);

    for (u32 inputIdx = 0; inputIdx < recipe->inputCount; ++inputIdx)
    {
        Item *input = recipe->inputs + inputIdx;

        u32 recipeCount = get_recipe_count(calculator, input->name);
        if (recipeCount)
        {
            Recipe *inputRecipe = get_recipe(calculator, input->name);
            calc_total_production(calculator, cost, inputRecipe, ratio * input->itemsPerMinute);
        }
    }
}

internal void
print_total_production(Calculator *calculator, FileStream output, CostTest *cost)
{
    for (u32 productionIdx = 0; productionIdx < cost->produceCount; ++productionIdx)
    {
        Item *produce = cost->producedItems + productionIdx;
        Item *consumed = get_consume_item(cost, produce->name);

        Recipe *productionRecipe = get_recipe(calculator, produce->name);
        i_expect(productionRecipe);

        if (consumed)
        {
            print_line(output, "Intermediate %.*s: %5.2f per minute (%3.1fx)", STR_FMT(produce->name), produce->itemsPerMinute, produce->itemsPerMinute / productionRecipe->output.itemsPerMinute);
            if (consumed->itemsPerMinute < produce->itemsPerMinute)
            {
                f32 extras = produce->itemsPerMinute - consumed->itemsPerMinute;
                ++output.indent;
                print_line(output, "Extras: %5.2f per minute", extras);
                --output.indent;
            }
        }
        else
        {
            print_line(output, "Producing %.*s: %5.2f per minute (%3.1fx)", STR_FMT(produce->name), produce->itemsPerMinute,
                       produce->itemsPerMinute / productionRecipe->output.itemsPerMinute);
        }
    }

    for (u32 consumptionIdx = 0; consumptionIdx < cost->consumeCount; ++consumptionIdx)
    {
        Item *consumed = cost->consumedItems + consumptionIdx;
        Item *produce = get_produce_item(cost, consumed->name);

        if (!produce)
        {
            print_line(output, "Consuming %.*s: %5.2f per minute", STR_FMT(consumed->name), consumed->itemsPerMinute);
        }
    }
}

internal void
print_recipe(Calculator *calculator, FileStream output, CostTest *cost, Recipe *endRecipe, f32 expectedPerMinute,
             b32 printAlternates = false, b32 printOverproduce = false)
{
    f32 ratio = expectedPerMinute / endRecipe->output.itemsPerMinute;

    //fprintf(stdout, "=====================================================\n");
    print_line(output, "%.*s: %5.2f per minute (%3.1fx)", STR_FMT(endRecipe->output.name), expectedPerMinute, ratio);

    if (printOverproduce)
    {
        //print_line(output, "%5.2f => %5.2f (%5.2f)\n", ratio, ceil(ratio), test);
        f32 newRatio = ceil(ratio);

        if (newRatio != ratio)
        {
            f32 newProduced = newRatio * endRecipe->output.itemsPerMinute;
            print_line(output, "%.*s: OVERPRODUCING: From %5.2f to %5.2f per minute (%3.1fx)", STR_FMT(endRecipe->output.name), expectedPerMinute, newProduced, newRatio);
            ratio = newRatio;
        }
    }
    add_recipe_cost(cost, endRecipe, ratio);

    //++output.indent;
    //print_line(output, "input%s:", (endRecipe->inputCount > 1) ? "s" : "");

    ++output.indent;
    for (u32 inputIdx = 0; inputIdx < endRecipe->inputCount; ++inputIdx)
    {
        Item *input = endRecipe->inputs + inputIdx;
        u32 recipeCount = get_recipe_count(calculator, input->name);
        if (recipeCount)
        {
            Recipe *recipe = get_recipe(calculator, input->name);

            f32 expectedInput = input->itemsPerMinute * ratio;
            if (printOverproduce)
            {
                Item *producedAlready = get_produce_item(cost, input->name);
                Item *consumedAlready = get_consume_item(cost, input->name);

                f32 consumedItems = 0.0f;
                if (consumedAlready)
                {
                    consumedItems = consumedAlready->itemsPerMinute;
                }

                f32 extraPerMinute = 0.0f;
                if (producedAlready)
                {
                    // NOTE(NAME): Double count the expectedInput, because of the way add_recipe_cost works
                    i_expect(consumedItems >= producedAlready->itemsPerMinute);
                    extraPerMinute = expectedInput + producedAlready->itemsPerMinute - consumedItems;
                }

                if (extraPerMinute >= expectedInput)
                {
                    if (!consumedAlready)
                    {
                        i_expect(cost->consumeCount < array_count(cost->consumedItems));
                        consumedAlready = cost->consumedItems + cost->consumeCount++;
                        consumedAlready->name = input->name;
                        consumedAlready->itemsPerMinute = 0;
                    }

                    consumedAlready->itemsPerMinute += expectedInput;
                    expectedInput = 0;
                }
                else if (extraPerMinute > 0.0f)
                {
                    if (!consumedAlready)
                    {
                        i_expect(cost->consumeCount < array_count(cost->consumedItems));
                        consumedAlready = cost->consumedItems + cost->consumeCount++;
                        consumedAlready->name = input->name;
                        consumedAlready->itemsPerMinute = 0;
                    }
                    print_line(output, "%.*s: USING EXTRA %5.2f per minute", STR_FMT(input->name), extraPerMinute);
                    consumedAlready->itemsPerMinute += extraPerMinute;
                    expectedInput = expectedInput - extraPerMinute;
                }
            }

            if (expectedInput > 0.0f)
            {
                print_recipe(calculator, output, cost, recipe, expectedInput, printAlternates, printOverproduce);

                if ((recipeCount > 1) && printAlternates) {
                    print_line(output, "alternates:");
                    CostTest fakeCost = {};
                    ++output.indent;
                    for (u32 skip = 1; skip < recipeCount; ++skip)
                    {
                        Recipe *recipe = get_recipe(calculator, input->name, skip);
                        i_expect(recipe);
                        print_recipe(calculator, output, &fakeCost, recipe, input->itemsPerMinute * ratio, false, printOverproduce);
                    }
                    --output.indent;
                }
            }
            else
            {
                print_line(output, "%.*s: already produced previously", STR_FMT(input->name));
            }
        }
        else
        {
            print_line(output, "%.*s: %5.2f per minute", STR_FMT(input->name), input->itemsPerMinute * ratio);
        }
    }
    --output.indent;

    //fprintf(stdout, "=====================================================\n");
}

internal String
print_dot_recipe(Calculator *calculator, FileStream output, Recipe *recipe, f32 expectedPerMinute,
                 u32 maxNameCount, u8 *nameData, u32 *index)
{
    u8 tempBuffer[256];
    u8 recordBuffer[1024];

    f32 ratio = expectedPerMinute / recipe->output.itemsPerMinute;

    String result = {0, nameData};
    String snakeName = to_snake(recipe->output.name, array_count(tempBuffer), tempBuffer);
    result = string_fmt(maxNameCount, nameData, "%.*s%d", STR_FMT(snakeName), (*index)++);

    String record = string_fmt(array_count(recordBuffer), recordBuffer, "%.*s [shape=record, label=\"{", STR_FMT(result));
    for (u32 inputIdx = 0; inputIdx < recipe->inputCount; ++inputIdx)
    {
        Item *input = recipe->inputs + inputIdx;
        snakeName = to_snake(input->name, array_count(tempBuffer), tempBuffer);
        record = append_string_fmt(record, array_count(recordBuffer), "%s<%.*s>%.*s", inputIdx == 0 ? "{" : "|", STR_FMT(snakeName), STR_FMT(input->name));
    }
    snakeName = to_snake(recipe->output.name, array_count(tempBuffer), tempBuffer); // TODO(michiel): I know...
    record = append_string_fmt(record, array_count(recordBuffer), "}|%3.1f|{<%.*s>%.*s", ratio, STR_FMT(snakeName), STR_FMT(recipe->output.name));
    if (recipe->extraOutput.name.size)
    {
        snakeName = to_snake(recipe->extraOutput.name, array_count(tempBuffer), tempBuffer);
        record = append_string_fmt(record, array_count(recordBuffer), "|<%.*s>%.*s", STR_FMT(snakeName), STR_FMT(recipe->extraOutput.name));
    }
    record = append_string(record, string("}}\"];"), array_count(recordBuffer));

    print_line(output, "%.*s", STR_FMT(record));


    u8 inputBuffer[256];
    for (u32 inputIdx = 0; inputIdx < recipe->inputCount; ++inputIdx)
    {
        Item *input = recipe->inputs + inputIdx;
        u32 recipeCount = get_recipe_count(calculator, input->name);
        if (recipeCount)
        {
            Recipe *inputRecipe = get_recipe(calculator, input->name);
            i_expect(inputRecipe);

            f32 expectedInput = input->itemsPerMinute * ratio;
            String inputString = print_dot_recipe(calculator, output, inputRecipe, expectedInput, array_count(inputBuffer), inputBuffer, index);

            snakeName = to_snake(input->name, array_count(tempBuffer), tempBuffer);
            print_line(output, "%.*s:%.*s -> %.*s:%.*s", STR_FMT(inputString), STR_FMT(snakeName), STR_FMT(result), STR_FMT(snakeName));
        }
    }

    return result;
}

internal void
print_dotfile(Calculator *calculator, FileStream output, Recipe *recipe, f32 expectedPerMinute)
{
    u8 tempBuffer[256];
    String camelName = to_camel(recipe->output.name, array_count(tempBuffer), tempBuffer);
    print_line(output, "digraph %.*s {", STR_FMT(camelName));
    ++output.indent;
    print_line(output, "rankdir=LR;");
    print_line(output, "ranksep=\"1\";\n");

    u32 index = 0;
    print_dot_recipe(calculator, output, recipe, expectedPerMinute, array_count(tempBuffer), tempBuffer, &index);

    --output.indent;
    print_line(output, "}");
}

int main(int argc, char **argv)
{
    Calculator calculator = {};

    calculator.maxRecipeCount = 1024;
    calculator.recipes = (Recipe *)malloc(sizeof(Recipe) * calculator.maxRecipeCount);
    //add_item(&calculator, static_string("unknown"));

    add_recipe(&calculator, Smelter, static_string("iron ingot"), 30.0f, static_string("iron ore"), 30.0f);
    add_recipe(&calculator, Smelter, static_string("copper ingot"), 30.0f, static_string("copper ore"), 30.0f);
    add_recipe(&calculator, Foundry, static_string("steel ingot"), 45.0f, static_string("iron ore"), 45.0f, static_string("coal"), 45.0f);
    add_recipe(&calculator, Smelter, static_string("caterium ingot"), 15.0f, static_string("caterium ore"), 45.0f);
    add_recipe(&calculator, Foundry, static_string("aluminium ingot"), 60.0f, static_string("aluminium scrap"), 90.0f, static_string("silica"), 75.0f);

    add_recipe(&calculator, Constructor, static_string("concrete"), 15.0f, static_string("limestone"), 45.0f);
    add_recipe(&calculator, Constructor, static_string("iron plate"), 20.0f, static_string("iron ingot"), 30.0f);
    add_recipe(&calculator, Constructor, static_string("iron rod"), 15.0f, static_string("iron ingot"), 15.0f);
    add_recipe(&calculator, Constructor, static_string("screw"), 40.0f, static_string("iron rod"), 10.0f);
    add_recipe(&calculator, Constructor, static_string("wire"), 30.0f, static_string("copper ingot"), 15.0f);
    add_recipe(&calculator, Constructor, static_string("copper sheet"), 10.0f, static_string("copper ingot"), 20.0f);
    add_recipe(&calculator, Constructor, static_string("cable"), 30.0f, static_string("wire"), 60.0f);
    add_recipe(&calculator, Constructor, static_string("steel beam"), 15.0f, static_string("steel ingot"), 60.0f);
    add_recipe(&calculator, Constructor, static_string("steel pipe"), 20.0f, static_string("steel ingot"), 30.0f);
    add_recipe(&calculator, Constructor, static_string("quickwire"), 60.0f, static_string("caterium ingot"), 12.0f);
    add_recipe(&calculator, Constructor, static_string("quartz crystal"), 22.5f, static_string("raw quartz"), 37.5f);
    add_recipe(&calculator, Constructor, static_string("silica"), 37.5f, static_string("raw quartz"), 22.5f);
    add_recipe(&calculator, Constructor, static_string("empty canister"), 60.0f, static_string("plastic"), 30.0f);
    add_recipe(&calculator, Constructor, static_string("aluminium casing"), 60.0f, static_string("aluminium ingot"), 90.0f);

    add_recipe(&calculator, Assembler, static_string("rotor"), 4.0f, static_string("iron rod"), 20.0f, static_string("screw"), 100.0f);
    add_recipe(&calculator, Assembler, static_string("stator"), 5.0f, static_string("steel pipe"), 15.0f, static_string("wire"), 40.0f);
    add_recipe(&calculator, Assembler, static_string("motor"), 5.0f, static_string("rotor"), 10.0f, static_string("stator"), 10.0f);
    add_recipe(&calculator, Assembler, static_string("modular frame"), 2.0f, static_string("reinforced iron plate"), 3.0f, static_string("iron rod"), 12.0f);
    add_recipe(&calculator, Assembler, static_string("reinforced iron plate"), 5.0f, static_string("iron plate"), 30.0f, static_string("screw"), 60.0f);
    add_recipe(&calculator, Assembler, static_string("encased industrial beam"), 6.0f, static_string("steel beam"), 24.0f, static_string("concrete"), 30.0f);
    add_recipe(&calculator, Assembler, static_string("ai limiter"), 5.0f, static_string("copper sheet"), 25.0f, static_string("quickwire"), 100.0f);
    add_recipe(&calculator, Assembler, static_string("circuit board"), 7.5f, static_string("copper sheet"), 15.0f, static_string("plastic"), 30.0f);
    add_recipe(&calculator, Assembler, static_string("fabric"), 15.0f, static_string("mycelia"), 15.0f, static_string("biomass"), 75.0f);
    add_recipe(&calculator, Assembler, static_string("alclad aluminium sheet"), 30.0f, static_string("aluminium ingot"), 60.0f, static_string("copper ingot"), 22.5f);
    add_recipe(&calculator, Assembler, static_string("heat sink"), 10.0f, static_string("alclad aluminium sheet"), 40.0f, static_string("rubber"), 70.0f);

    add_recipe(&calculator, Manufacturer, static_string("beacon"), 7.5f, static_string("iron plate"), 22.5f, static_string("iron rod"), 7.5f, static_string("wire"), 112.5f, static_string("cable"), 15.0f);
    add_recipe(&calculator, Manufacturer, static_string("crystal oscillator"), 1.0f, static_string("quartz crystal"), 18.0f, static_string("cable"), 14.0f, static_string("reinforced iron plate"), 2.5f);
    add_recipe(&calculator, Manufacturer, static_string("heavy modular frame"), 2.0f, static_string("modular frame"), 10.0f, static_string("steel pipe"), 30.0f, static_string("encased industrial beam"), 10.0f, static_string("screw"), 200.0f);
    add_recipe(&calculator, Manufacturer, static_string("computer"), 2.5f, static_string("circuit board"), 25.0f, static_string("cable"), 22.5f, static_string("plastic"), 45.0f, static_string("screw"), 130.0f);
    add_recipe(&calculator, Manufacturer, static_string("high-speed connector"), 3.8f, static_string("quickwire"), 210.0f, static_string("cable"), 37.5f, static_string("circuit board"), 3.75f);
    add_recipe(&calculator, Manufacturer, static_string("supercomputer"), 1.875f, static_string("computer"), 3.75f, static_string("ai limiter"), 3.75f, static_string("high-speed connector"), 5.625f, static_string("plastic"), 52.5f);
    add_recipe(&calculator, Manufacturer, static_string("battery"), 5.625f, static_string("alclad aluminium sheet"), 15.0f, static_string("wire"), 30.0f, static_string("sulfur"), 37.5f, static_string("plastic"), 15.0f);
    add_recipe(&calculator, Manufacturer, static_string("radio control unit"), 2.5f, static_string("aluminium casing"), 40.0f, static_string("crystal oscillator"), 1.25f, static_string("computer"), 1.25f);
    add_recipe(&calculator, Manufacturer, static_string("turbo motor"), 1.875f, static_string("heat sink"), 7.5f, static_string("radio control unit"), 3.75f, static_string("motor"), 7.5f, static_string("rubber"), 45.0f);

    add_recipe(&calculator, Assembler, static_string("compacted coal"), 25.0f, static_string("coal"), 25.0f, static_string("sulfur"), 25.0f);
    add_recipe(&calculator, Assembler, static_string("black powder"), 7.5f, static_string("coal"), 7.5f, static_string("sulfur"), 15.0f);
    add_recipe(&calculator, Assembler, static_string("nobelisk"), 3.0f, static_string("black powder"), 15.0f, static_string("steel pipe"), 30.0f);
    add_recipe(&calculator, Manufacturer, static_string("gas filter"), 7.5f, static_string("coal"), 5.0f, static_string("rubber"), 15.0f, static_string("fabric"), 15.0f);
    add_recipe(&calculator, Manufacturer, static_string("rifle cartridge"), 15.0f, static_string("beacon"), 3.0f, static_string("steel pipe"), 30.0f, static_string("black powder"), 30.0f, static_string("rubber"), 30.0f);

    Recipe *plastic = add_recipe(&calculator, Refinery, static_string("plastic"), 20.0f, static_string("crude oil"), 30.0f);
    add_extra_item(&calculator, plastic, static_string("heavy oil residue"), 10.0f);
    add_recipe(&calculator, Refinery, static_string("plastic"), 20.0f, static_string("polymer resin"), 60.0f, static_string("water"), 20.0f);

    Recipe *rubber = add_recipe(&calculator, Refinery, static_string("rubber"), 20.0f, static_string("crude oil"), 30.0f);
    add_extra_item(&calculator, rubber, static_string("heavy oil residue"), 20.0f);
    add_recipe(&calculator, Refinery, static_string("rubber"), 20.0f, static_string("polymer resin"), 40.0f, static_string("water"), 40.0f);

    Recipe *fuel = add_recipe(&calculator, Refinery, static_string("fuel"), 40.0f, static_string("crude oil"), 60.0f);
    add_extra_item(&calculator, fuel, static_string("polymer resin"), 30.0f);
    add_recipe(&calculator, Refinery, static_string("fuel"), 40.0f, static_string("heavy oil residue"), 60.0f);
    add_recipe(&calculator, Refinery, static_string("turbofuel"), 18.75f, static_string("fuel"), 22.5f, static_string("compacted coal"), 15.0f);

    add_recipe(&calculator, Refinery, static_string("petroleum coke"), 120.0f, static_string("heavy oil residue"), 40.0f);

    Recipe *alumina = add_recipe(&calculator, Refinery, static_string("alumina solution"), 120.0f, static_string("bauxite"), 120.0f, static_string("water"), 180.0f);
    add_extra_item(&calculator, alumina, static_string("silica"), 50.0f);

    Recipe *aluScrap = add_recipe(&calculator, Refinery, static_string("aluminium scrap"), 360.0f, static_string("alumina solution"), 240.0f, static_string("coal"), 120.0f);
    add_extra_item(&calculator, aluScrap, static_string("water"), 120.0f);

    add_recipe(&calculator, Refinery, static_string("sulfuric acid"), 100.0f, static_string("sulfur"), 50.0f, static_string("water"), 50.0f);
    Recipe *uranPellet = add_recipe(&calculator, Refinery, static_string("uranium pellet"), 50.0f, static_string("uranium"), 50.0f, static_string("sulfuric acid"), 80.0f);
    add_extra_item(&calculator, uranPellet, static_string("sulfuric acid"), 20.0f);

    add_recipe(&calculator, Packager, static_string("packaged water"), 60.0f, static_string("water"), 60.0f, static_string("empty canister"), 60.0f);
    add_recipe(&calculator, Packager, static_string("packaged oil"), 30.0f, static_string("crude oil"), 30.0f, static_string("empty canister"), 30.0f);
    add_recipe(&calculator, Packager, static_string("packaged heavy oil residue"), 30.0f, static_string("heavy oil residue"), 30.0f, static_string("empty canister"), 30.0f);
    add_recipe(&calculator, Packager, static_string("packaged fuel"), 40.0f, static_string("fuel"), 40.0f, static_string("empty canister"), 40.0f);
    add_recipe(&calculator, Packager, static_string("packaged turbofuel"), 20.0f, static_string("turbofuel"), 20.0f, static_string("empty canister"), 20.0f);
    add_recipe(&calculator, Packager, static_string("packaged alumina solution"), 120.0f, static_string("alumina solution"), 120.0f, static_string("empty canister"), 120.0f);

    add_recipe(&calculator, Assembler, static_string("encased uranium cell"), 10.0f, static_string("uranium pellet"), 40.0f, static_string("concrete"), 9.0f);
    add_recipe(&calculator, Assembler, static_string("electromagnetic control rod"), 4.0f, static_string("stator"), 6.0f, static_string("ai limiter"), 4.0f);
    add_recipe(&calculator, Manufacturer, static_string("nuclear fuel rod"), 0.4f, static_string("encased uranium cell"), 10.0f, static_string("encased industrial beam"), 1.2f, static_string("electromagnetic control rod"), 2.0f);

    add_recipe(&calculator, Assembler, static_string("automated wiring"), 2.5f, static_string("stator"), 2.5f, static_string("cable"), 50.0f);
    add_recipe(&calculator, Assembler, static_string("smart plating"), 2.0f, static_string("reinforced iron plate"), 2.0f, static_string("rotor"), 2.0f);
    add_recipe(&calculator, Assembler, static_string("versatile framework"), 5.0f, static_string("modular frame"), 2.5f, static_string("steel beam"), 30.0f);
    add_recipe(&calculator, Manufacturer, static_string("modular engine"), 1.0f, static_string("motor"), 2.0f, static_string("rubber"), 15.0f, static_string("smart plating"), 2.0f);
    add_recipe(&calculator, Manufacturer, static_string("adaptive control unit"), 1.0f, static_string("automated wiring"), 7.5f, static_string("circuit board"), 5.0f, static_string("heavy modular frame"), 1.0f, static_string("computer"), 1.0f);

    // NOTE(michiel): Alternates

    add_recipe(&calculator, Foundry, static_string("copper ingot"), 100.0f, static_string("copper ore"), 50.0f, static_string("iron ore"), 25.0f);
    add_recipe(&calculator, Foundry, static_string("steel ingot"), 60.0f, static_string("iron ingot"), 40.0f, static_string("coal"), 40.0f);
    add_recipe(&calculator, Constructor, static_string("screw"), 50.0f, static_string("iron ingot"), 12.5f);
    add_recipe(&calculator, Constructor, static_string("screw"), 260.0f, static_string("steel beam"), 5.0f);
    add_recipe(&calculator, Constructor, static_string("wire"), 22.5f, static_string("iron ingot"), 12.5f);
    add_recipe(&calculator, Assembler, static_string("quickwire"), 90.0f, static_string("caterium ingot"), 7.5f, static_string("copper ingot"), 37.5f);
    add_recipe(&calculator, Assembler, static_string("circuit board"), 5.0f, static_string("rubber"), 30.0f, static_string("petroleum coke"), 45.0f);
    add_recipe(&calculator, Assembler, static_string("encased industrial beam"), 4.0f, static_string("steel pipe"), 28.0f, static_string("concrete"), 20.0f);
    add_recipe(&calculator, Assembler, static_string("computer"), 2.8125f, static_string("circuit board"), 7.5f, static_string("crystal oscillator"), 2.8125f);
    add_recipe(&calculator, Assembler, static_string("empty canister"), 60.0f, static_string("iron plate"), 30.0f, static_string("copper sheet"), 15.0f);
    add_recipe(&calculator, Assembler, static_string("reinforced iron plate"), 15.0f, static_string("iron plate"), 90.0f, static_string("screw"), 250.0f);
    add_recipe(&calculator, Assembler, static_string("reinforced iron plate"), 5.625f, static_string("iron plate"), 18.75f, static_string("wire"), 37.5f);
    add_recipe(&calculator, Assembler, static_string("black powder"), 15.0f, static_string("compacted coal"), 3.75f, static_string("sulfur"), 7.5f);
    add_recipe(&calculator, Manufacturer, static_string("computer"), 3.75f, static_string("circuit board"), 26.25f, static_string("quickwire"), 105.0f, static_string("rubber"), 45.0f);
    add_recipe(&calculator, Manufacturer, static_string("crystal oscillator"), 1.875f, static_string("quartz crystal"), 18.75f, static_string("rubber"), 13.125f, static_string("ai limiter"), 1.875f);
    add_recipe(&calculator, Manufacturer, static_string("heavy modular frame"), 2.8125f, static_string("modular frame"), 7.5f, static_string("encased industrial beam"), 9.375f, static_string("steel pipe"), 33.75f, static_string("concrete"), 20.625f);
    add_recipe(&calculator, Refinery, static_string("plastic"), 60.0f, static_string("rubber"), 30.0f, static_string("fuel"), 30.0f);
    add_recipe(&calculator, Refinery, static_string("rubber"), 60.0f, static_string("plastic"), 30.0f, static_string("fuel"), 30.0f);
    Recipe *heavyResidue = add_recipe(&calculator, Refinery, static_string("heavy oil residue"), 40.0f, static_string("crude oil"), 30.0f);
    add_extra_item(&calculator, heavyResidue, static_string("polymer resin"), 20.0f);
    add_recipe(&calculator, Refinery, static_string("packaged fuel"), 60.0f, static_string("heavy oil residue"), 30.0f, static_string("packaged water"), 60.0f);
    add_recipe(&calculator, Refinery, static_string("quartz crystal"), 52.5f, static_string("raw quartz"), 67.5f, static_string("water"), 37.5f);
    add_recipe(&calculator, Refinery, static_string("copper sheet"), 22.5f, static_string("copper ingot"), 22.5f, static_string("water"), 22.5f);


    CostTest *cost = allocate_struct(CostTest);

    b32 printAlternates = false;
    b32 printResources = false;
    b32 printOverproduce = false;
    b32 printTotal = false;
    b32 printDot = false;
    String recipeName = {};
    f32 expectedAmount = 0.0f;

    u32 togo = argc - 1;
    if (togo)
    {
        char **arguments = argv + 1;
        while (togo)
        {
            if ((togo > 1) && (arguments[0][0] == '-')) {
                if (arguments[0][1] == 'a') {
                    printAlternates = true;
                } else if (arguments[0][1] == 'r') {
                    printResources = true;
                } else if (arguments[0][1] == 'o') {
                    printOverproduce = true;
                } else if (arguments[0][1] == 't') {
                    printTotal = true;
                } else if (arguments[0][1] == 'd') {
                    printDot = true;
                }
            } else if (recipeName.size == 0) {
                recipeName = string(arguments[0]);
            } else {
                expectedAmount = float_from_string(string(arguments[0]));
            }
            --togo;
            ++arguments;
        }

        Recipe *recipe = get_recipe(&calculator, recipeName);
        if (recipe)
        {
            f32 expectedCalc = expectedAmount;
            if (expectedCalc == 0.0f) {
                expectedCalc = recipe->output.itemsPerMinute;
            }
            FileStream outputStream = {};
            outputStream.file.platform = stdout;
            outputStream.file.noErrors = 1;
            outputStream.file.filename = static_string("stdout");

            if (printDot)
            {
                print_dotfile(&calculator, outputStream, recipe, expectedCalc);
            }
            else if (printTotal)
            {
                calc_total_production(&calculator, cost, recipe, expectedCalc);
                print_total_production(&calculator, outputStream, cost);
            }
            else
            {
                u32 recipeCount = get_recipe_count(&calculator, recipeName);
                for (u32 index = 0; index < recipeCount; ++index) {
                    recipe = get_recipe(&calculator, recipeName, index);
                    if (index > 0) {
                        fprintf(stdout, "\n\nALTERNATE:\n");
                    }
                    if (expectedAmount == 0.0f) {
                        expectedCalc = recipe->output.itemsPerMinute;
                    }
                    print_recipe(&calculator, outputStream, cost, recipe, expectedCalc, printAlternates, printOverproduce);
                    fprintf(stdout, "\n");
                    if (printResources) {
                        print_cost(cost);
                        fprintf(stdout, "\n");
                    }
                    output_input_cost(cost);
                    print_cost(cost);
                    *cost = {};
                }
            }
        }
        else
        {
            // NOTE(NAME): Very crude string comparator to help spelling mistakes
            u32 bestMatchCount = 0;
            String bestMatch = {};

            for (u32 recipeIdx = 0; recipeIdx < calculator.recipeCount; ++recipeIdx)
            {
                Recipe *recipe = calculator.recipes + recipeIdx;
                String testName = recipe->output.name;
                u32 searchMult = 1;
                u32 searchSum = 0;
                u32 matchIdx = 0;
                for (u32 idx = 0; (idx < testName.size) && (matchIdx < recipeName.size); ++idx)
                {
                    if (to_lower_case(recipeName.data[matchIdx]) == to_lower_case(testName.data[idx]))
                    {
                        searchSum += searchMult;
                        searchMult *= 2;
                        ++matchIdx;
                    }
                    else
                    {
                        searchMult = 1;
                    }
                }
                if (bestMatchCount < searchSum)
                {
                    bestMatchCount = searchSum;
                    bestMatch = testName;
                }
            }
            fprintf(stderr, "Recipe '%.*s' not found! Did you mean '%.*s'?\n", STR_FMT(recipeName), STR_FMT(bestMatch));
        }
    }
    else
    {
        fprintf(stderr, "Usage: %s <recipe name> [items per minute]\n", argv[0]);
    }

    return 0;
}
