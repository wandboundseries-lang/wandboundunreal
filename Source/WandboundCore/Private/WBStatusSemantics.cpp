#include "WBStatusSemantics.h"

#include "WBGameStateData.h"

namespace
{
FString ComparableStatusId(const FName StatusId)
{
	return StatusId.IsNone()
		? FString()
		: StatusId.GetPlainNameString().ToLower();
}
}

FName WBStatusSemantics::CanonicalizeStatusId(const FName StatusId)
{
	const FString Id = ComparableStatusId(StatusId);
	if (Id == TEXT("burn"))
	{
		return FName(TEXT("Burn"));
	}
	if (Id == TEXT("poison"))
	{
		return FName(TEXT("Poison"));
	}
	if (Id == TEXT("root") || Id == TEXT("rooted"))
	{
		return FName(TEXT("Rooted"));
	}
	if (Id == TEXT("stun") || Id == TEXT("stunned"))
	{
		return FName(TEXT("Stunned"));
	}
	if (Id == TEXT("frozen"))
	{
		return FName(TEXT("Frozen"));
	}
	if (Id == TEXT("cannotattack")
		|| Id == TEXT("cannot attack")
		|| Id == TEXT("cannot_attack")
		|| Id == TEXT("no_attack")
		|| Id == TEXT("cant_attack")
		|| Id == TEXT("attack_disabled")
		|| Id == TEXT("disarmed"))
	{
		return FName(TEXT("Cannot Attack"));
	}
	if (Id == TEXT("sealed"))
	{
		return FName(TEXT("Sealed"));
	}
	return StatusId;
}

bool WBStatusSemantics::IsCanonicalStatusId(const FName StatusId)
{
	const FName Canonical = CanonicalizeStatusId(StatusId);
	return Canonical == FName(TEXT("Burn"))
		|| Canonical == FName(TEXT("Poison"))
		|| Canonical == FName(TEXT("Rooted"))
		|| Canonical == FName(TEXT("Stunned"))
		|| Canonical == FName(TEXT("Frozen"))
		|| Canonical == FName(TEXT("Cannot Attack"));
}

bool WBStatusSemantics::IsDeferredStatusId(const FName StatusId)
{
	return CanonicalizeStatusId(StatusId) == FName(TEXT("Sealed"));
}

bool WBStatusSemantics::IsCleanseableStatusId(const FName StatusId)
{
	return IsCanonicalStatusId(StatusId);
}

TArray<FName> WBStatusSemantics::GetCanonicalStatusIds()
{
	return {
		FName(TEXT("Burn")),
		FName(TEXT("Cannot Attack")),
		FName(TEXT("Frozen")),
		FName(TEXT("Poison")),
		FName(TEXT("Rooted")),
		FName(TEXT("Stunned"))
	};
}

bool WBStatusSemantics::HasCanonicalStatus(
	const FWBUnitState& Unit,
	const FName StatusId)
{
	return Unit.HasStatus(CanonicalizeStatusId(StatusId));
}

bool WBStatusSemantics::CanDeclareMove(const FWBUnitState& Unit)
{
	return !HasCanonicalStatus(Unit, FName(TEXT("Rooted")))
		&& !HasCanonicalStatus(Unit, FName(TEXT("Stunned")))
		&& !HasCanonicalStatus(Unit, FName(TEXT("Frozen")));
}

bool WBStatusSemantics::CanDeclareAttack(const FWBUnitState& Unit)
{
	return !HasCanonicalStatus(Unit, FName(TEXT("Stunned")))
		&& !HasCanonicalStatus(Unit, FName(TEXT("Frozen")))
		&& !HasCanonicalStatus(Unit, FName(TEXT("Cannot Attack")));
}

bool WBStatusSemantics::CanCounterattack(const FWBUnitState& Unit)
{
	return !HasCanonicalStatus(Unit, FName(TEXT("Stunned")))
		&& !HasCanonicalStatus(Unit, FName(TEXT("Frozen")));
}

bool WBStatusSemantics::CanUseUnitActivation(const FWBUnitState& Unit)
{
	return !HasCanonicalStatus(Unit, FName(TEXT("Stunned")))
		&& !HasCanonicalStatus(Unit, FName(TEXT("Frozen")));
}

bool WBStatusSemantics::CanUseAutomaticCharacterPassive(const FWBUnitState& Unit)
{
	return CanUseUnitActivation(Unit)
		&& !Unit.HasStatus(FName(TEXT("Negated")));
}
