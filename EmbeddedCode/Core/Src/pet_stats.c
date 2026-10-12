
#include "pet_stats.h"

/* Internal helper: keep a statistic within 0-100. */
static uint8_t ClampStat(int value)
{
    if (value < PET_STAT_MIN)
    {
        return PET_STAT_MIN;
    }

    if (value > PET_STAT_MAX)
    {
        return PET_STAT_MAX;
    }

    return (uint8_t)value;
}

/* Determine whether the pet has reached an ending condition. */
static void CheckStatus(PetStats *pet)
{
    if (pet->hunger >= PET_HUNGER_LIMIT)
    {
        pet->status = PET_DEAD;
    }
    else if (pet->happiness <= PET_HAPPINESS_LIMIT)
    {
        pet->status = PET_RAN_AWAY;
    }
}

void PetStats_Init(PetStats *pet)
{
    if (pet == 0)
    {
        return;
    }

    pet->hunger = 0;
    pet->happiness = 100;
    pet->status = PET_ALIVE;
}

void PetStats_Update(PetStats *pet)
{
    if ((pet == 0) || (pet->status != PET_ALIVE))
    {
        return;
    }

    /* Each update represents one second of game time. */
    pet->hunger = ClampStat(pet->hunger + 1);
    pet->happiness = ClampStat(pet->happiness - 1);

    CheckStatus(pet);
}

void PetStats_Feed(PetStats *pet)
{
    if ((pet == 0) || (pet->status != PET_ALIVE))
    {
        return;
    }

    /* Feeding reduces hunger and slightly improves happiness. */
    pet->hunger = ClampStat(pet->hunger - 20);
    pet->happiness = ClampStat(pet->happiness + 5);

    CheckStatus(pet);
}

void PetStats_Play(PetStats *pet)
{
    if ((pet == 0) || (pet->status != PET_ALIVE))
    {
        return;
    }

    /* Playing improves happiness but makes the pet hungrier. */
    pet->happiness = ClampStat(pet->happiness + 15);
    pet->hunger = ClampStat(pet->hunger + 5);

    CheckStatus(pet);
}
