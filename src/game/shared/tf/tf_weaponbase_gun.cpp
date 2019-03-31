//====== Copyright © 1996-2005, Valve Corporation, All rights reserved. =======
//
// Purpose: Weapon Base Gun 
//
//=============================================================================

#include "cbase.h"
#include "tf_weaponbase_gun.h"
#include "tf_fx_shared.h"
#include "effect_dispatch_data.h"
#include "takedamageinfo.h"
#include "tf_projectile_nail.h"
#include "tf_projectile_arrow.h"
#include "tf_projectile_jar.h"

#if !defined( CLIENT_DLL )	// Server specific.

	#include "tf_gamestats.h"
	#include "tf_player.h"
	#include "tf_fx.h"
	#include "te_effect_dispatch.h"

	#include "tf_projectile_rocket.h"
	#include "tf_weapon_grenade_pipebomb.h"
	#include "tf_projectile_flare.h"
	#include "te.h"

#else	// Client specific.

	#include "c_tf_player.h"
	#include "c_te_effect_dispatch.h"

#endif

//=============================================================================
//
// TFWeaponBase Gun tables.
//
IMPLEMENT_NETWORKCLASS_ALIASED(TFWeaponBaseGun, DT_TFWeaponBaseGun)

BEGIN_NETWORK_TABLE(CTFWeaponBaseGun, DT_TFWeaponBaseGun)
END_NETWORK_TABLE()

BEGIN_PREDICTION_DATA(CTFWeaponBaseGun)
END_PREDICTION_DATA()

// Server specific.
#if !defined( CLIENT_DLL ) 
BEGIN_DATADESC( CTFWeaponBaseGun )
DEFINE_THINKFUNC( ZoomOutIn ),
DEFINE_THINKFUNC( ZoomOut ),
DEFINE_THINKFUNC( ZoomIn ),
END_DATADESC()
#endif

//=============================================================================
//
// TFWeaponBase Gun functions.
//

//-----------------------------------------------------------------------------
// Purpose: Constructor.
//-----------------------------------------------------------------------------
CTFWeaponBaseGun::CTFWeaponBaseGun()
{
	m_iWeaponMode = TF_WEAPON_PRIMARY_MODE;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CTFWeaponBaseGun::PrimaryAttack( void )
{
	// Check for ammunition.
	if ( m_iClip1 <= 0 && UsesClipsForAmmo1() )
		return;

	// Are we capable of firing again?
	if ( m_flNextPrimaryAttack > gpGlobals->curtime )
		return;

	// Get the player owning the weapon.
	CTFPlayer *pPlayer = ToTFPlayer( GetPlayerOwner() );
	if ( !pPlayer )
		return;

	if ( !CanAttack() )
		return;

	CalcIsAttackCritical();

#ifndef CLIENT_DLL
	pPlayer->RemoveInvisibility();
	pPlayer->RemoveDisguise();

	// Minigun has custom handling
	if ( GetWeaponID() != TF_WEAPON_MINIGUN )
	{
		pPlayer->SpeakWeaponFire();
	}
	CTF_GameStats.Event_PlayerFiredWeapon( pPlayer, IsCurrentAttackACrit() );
#endif

	// Set the weapon mode.
	m_iWeaponMode = TF_WEAPON_PRIMARY_MODE;

	SendWeaponAnim( ACT_VM_PRIMARYATTACK );

	pPlayer->SetAnimation( PLAYER_ATTACK1 );

	FireProjectile( pPlayer );

	m_flLastFireTime  = gpGlobals->curtime;

	// Set next attack times.
	float flFireDelay = m_pWeaponInfo->GetWeaponData( m_iWeaponMode ).m_flTimeFireDelay;
	CALL_ATTRIB_HOOK_FLOAT( flFireDelay, mult_postfiredelay );

	m_flNextPrimaryAttack = gpGlobals->curtime + flFireDelay;

	// Don't push out secondary attack, because our secondary fire
	// systems are all separate from primary fire (sniper zooming, demoman pipebomb detonating, etc)
	//m_flNextSecondaryAttack = gpGlobals->curtime + m_pWeaponInfo->GetWeaponData( m_iWeaponMode ).m_flTimeFireDelay;

	// Set the idle animation times based on the sequence duration, so that we play full fire animations
	// that last longer than the refire rate may allow.
	SetWeaponIdleTime( gpGlobals->curtime + SequenceDuration() );

	AbortReload();
}	

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CTFWeaponBaseGun::SecondaryAttack( void )
{
	// semi-auto behaviour
	if ( m_bInAttack2 )
		return;

	// Get the player owning the weapon.
	CTFPlayer *pPlayer = ToTFPlayer( GetPlayerOwner() );
	if ( !pPlayer )
		return;

	pPlayer->DoClassSpecialSkill();

	m_bInAttack2 = true;

	m_flNextSecondaryAttack = gpGlobals->curtime + 0.5;
}

CBaseEntity *CTFWeaponBaseGun::FireProjectile( CTFPlayer *pPlayer )
{
	int iProjectile = TF_PROJECTILE_NONE;

	CALL_ATTRIB_HOOK_INT( iProjectile, override_projectile_type );

	if ( !iProjectile )
		iProjectile = m_pWeaponInfo->GetWeaponData( m_iWeaponMode ).m_iProjectile;

	CBaseEntity *pProjectile = NULL;

	switch( iProjectile )
	{
	case TF_PROJECTILE_BULLET:
		FireBullet( pPlayer );
		break;

	case TF_PROJECTILE_ROCKET:
		pProjectile = FireRocket( pPlayer );
		pPlayer->DoAnimationEvent( PLAYERANIMEVENT_ATTACK_PRIMARY );
		break;

	case TF_PROJECTILE_SYRINGE:
		pProjectile = FireNail( pPlayer, iProjectile );
		pPlayer->DoAnimationEvent( PLAYERANIMEVENT_ATTACK_PRIMARY );
		break;

	case TF_PROJECTILE_PIPEBOMB:
	case TF_PROJECTILE_CANNONBALL:
		pProjectile = FirePipeBomb( pPlayer, false );
		pPlayer->DoAnimationEvent( PLAYERANIMEVENT_ATTACK_PRIMARY );
		break;

	case TF_PROJECTILE_PIPEBOMB_REMOTE:
	case TF_PROJECTILE_PIPEBOMB_REMOTE_PRACTICE:
		pProjectile = FirePipeBomb( pPlayer, true );
		pPlayer->DoAnimationEvent( PLAYERANIMEVENT_ATTACK_PRIMARY );
		break;

	case TF_PROJECTILE_FLARE:
		pProjectile = FireFlare(pPlayer);
		pPlayer->DoAnimationEvent(PLAYERANIMEVENT_ATTACK_PRIMARY);
		break;

	case TF_PROJECTILE_JAR:
	case TF_PROJECTILE_JAR_MILK:
	case TF_PROJECTILE_FESTITIVE_URINE:
	case TF_PROJECTILE_BREADMONSTER_JARATE:
	case TF_PROJECTILE_BREADMONSTER_MADMILK:
		pProjectile = FireJar(pPlayer, iProjectile);
		pPlayer->DoAnimationEvent(PLAYERANIMEVENT_ATTACK_PRIMARY);
		break;

	case TF_PROJECTILE_ARROW:
	case TF_PROJECTILE_HEALING_BOLT:
	case TF_PROJECTILE_BUILDING_REPAIR_BOLT:
	case TF_PROJECTILE_FESTITIVE_ARROW:
	case TF_PROJECTILE_FESTITIVE_HEALING_BOLT:
	case TF_PROJECTILE_GRAPPLINGHOOK:
		pProjectile = FireArrow(pPlayer, iProjectile);
		pPlayer->DoAnimationEvent(PLAYERANIMEVENT_ATTACK_PRIMARY);
		break;

	case TF_PROJECTILE_CLEAVER:
	case TF_PROJECTILE_THROWABLE:
	case TF_PROJECTILE_NONE:
	default:
		// do nothing!
		DevMsg( "Weapon does not have a projectile type set\n" );
		break;
	}

	DoFireEffects();

	RemoveAmmo( pPlayer );

	UpdatePunchAngles( pPlayer );

	return pProjectile;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFWeaponBaseGun::UpdatePunchAngles( CTFPlayer *pPlayer )
{
	// Update the player's punch angle.
	QAngle angle = pPlayer->GetPunchAngle();
	float flPunchAngle = m_pWeaponInfo->GetWeaponData( m_iWeaponMode ).m_flPunchAngle;

	if ( flPunchAngle > 0 )
	{
		angle.x -= SharedRandomInt( "ShotgunPunchAngle", ( flPunchAngle - 1 ), ( flPunchAngle + 1 ) );
		pPlayer->SetPunchAngle( angle );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Fire a bullet!
//-----------------------------------------------------------------------------
void CTFWeaponBaseGun::FireBullet( CTFPlayer *pPlayer )
{
	PlayWeaponShootSound();

	FX_FireBullets(
		pPlayer->entindex(),
		pPlayer->Weapon_ShootPosition(),
		pPlayer->EyeAngles() + pPlayer->GetPunchAngle(),
		GetWeaponID(),
		m_iWeaponMode,
		CBaseEntity::GetPredictionRandomSeed() & 255,
		GetWeaponSpread(),
		GetProjectileDamage(),
		IsCurrentAttackACrit() );
}

class CTraceFilterIgnoreTeammates : public CTraceFilterSimple
{
public:
	// It does have a base, but we'll never network anything below here..
	DECLARE_CLASS( CTraceFilterIgnoreTeammates, CTraceFilterSimple );

	CTraceFilterIgnoreTeammates( const IHandleEntity *passentity, int collisionGroup, int iIgnoreTeam )
		: CTraceFilterSimple( passentity, collisionGroup ), m_iIgnoreTeam( iIgnoreTeam )
	{
	}

	virtual bool ShouldHitEntity( IHandleEntity *pServerEntity, int contentsMask )
	{
		CBaseEntity *pEntity = EntityFromEntityHandle( pServerEntity );

		if ( pEntity->IsPlayer() && pEntity->GetTeamNumber() == m_iIgnoreTeam )
		{
			return false;
		}

		return true;
	}

	int m_iIgnoreTeam;
};

//-----------------------------------------------------------------------------
// Purpose: Return angles for a projectile reflected by airblast
//-----------------------------------------------------------------------------
void CTFWeaponBaseGun::GetProjectileReflectSetup( CTFPlayer *pPlayer, const Vector &vecPos, Vector *vecDeflect, bool bHitTeammates /* = true */, bool bUseHitboxes /* = false */ )
{
	Vector vecForward, vecRight, vecUp;
	AngleVectors( pPlayer->EyeAngles(), &vecForward, &vecRight, &vecUp );

	Vector vecShootPos = pPlayer->Weapon_ShootPosition();

	// Estimate end point
	Vector endPos = vecShootPos + vecForward * 2000;

	// Trace forward and find what's in front of us, and aim at that
	trace_t tr;
	int nMask = bUseHitboxes ? MASK_SOLID | CONTENTS_HITBOX : MASK_SOLID;

	if ( bHitTeammates )
	{
		CTraceFilterSimple filter( pPlayer, COLLISION_GROUP_NONE );
		UTIL_TraceLine( vecShootPos, endPos, nMask, &filter, &tr );
	}
	else
	{
		CTraceFilterIgnoreTeammates filter( pPlayer, COLLISION_GROUP_NONE, pPlayer->GetTeamNumber() );
		UTIL_TraceLine( vecShootPos, endPos, nMask, &filter, &tr );
	}

	// vecPos is projectile's current position. Use that to find angles.

	// Find angles that will get us to our desired end point
	// Only use the trace end if it wasn't too close, which results
	// in visually bizarre forward angles
	//if ( tr.fraction > 0.1 || bUseHitboxes )
	if ( tr.fraction > 0.1 )
	{
		*vecDeflect = tr.endpos - vecPos;
	}
	else
	{
		*vecDeflect = endPos - vecPos;
	}

	VectorNormalize( *vecDeflect );
}

//-----------------------------------------------------------------------------
// Purpose: Return the origin & angles for a projectile fired from the player's gun
//-----------------------------------------------------------------------------
void CTFWeaponBaseGun::GetProjectileFireSetup( CTFPlayer *pPlayer, Vector vecOffset, Vector *vecSrc, QAngle *angForward, bool bHitTeammates /* = true */, bool bUseHitboxes /* = false */ )
{
	Vector vecForward, vecRight, vecUp;
	AngleVectors( pPlayer->EyeAngles(), &vecForward, &vecRight, &vecUp );

	Vector vecShootPos = pPlayer->Weapon_ShootPosition();

	// Estimate end point
	Vector endPos = vecShootPos + vecForward * 2000;	

	// Trace forward and find what's in front of us, and aim at that
	trace_t tr;
	int nMask = bUseHitboxes ? MASK_SOLID | CONTENTS_HITBOX : MASK_SOLID;

	if ( bHitTeammates )
	{
		CTraceFilterSimple filter( pPlayer, COLLISION_GROUP_NONE );
		UTIL_TraceLine( vecShootPos, endPos, nMask, &filter, &tr );
	}
	else
	{
		CTraceFilterIgnoreTeammates filter( pPlayer, COLLISION_GROUP_NONE, pPlayer->GetTeamNumber() );
		UTIL_TraceLine( vecShootPos, endPos, nMask, &filter, &tr );
	}

#ifndef CLIENT_DLL
	// If viewmodel is flipped fire from the other side.
	if ( IsViewModelFlipped() )
	{
		vecOffset.y *= -1.0f;
	}

	// Offset actual start point
	*vecSrc = vecShootPos + (vecForward * vecOffset.x) + (vecRight * vecOffset.y) + (vecUp * vecOffset.z);
#else
	// If we're seeing another player shooting the projectile, move their start point to the weapon origin
	if ( pPlayer )
	{
		if ( !UsingViewModel() )
		{
			GetAttachment( "muzzle", *vecSrc );
		}
		else
		{
			C_BaseEntity *pViewModel = pPlayer->GetViewModel();

			if ( pViewModel )
			{
				QAngle vecAngles;
				int iMuzzleFlashAttachment = pViewModel->LookupAttachment( "muzzle" );
				pViewModel->GetAttachment( iMuzzleFlashAttachment, *vecSrc, vecAngles );

				Vector vForward;
				AngleVectors( vecAngles, &vForward );

				trace_t trace;	
				UTIL_TraceLine( *vecSrc + vForward * -50, *vecSrc, MASK_SOLID, pPlayer, COLLISION_GROUP_NONE, &trace );

				*vecSrc = trace.endpos;
			}
		}
	}
#endif

	// Find angles that will get us to our desired end point
	// Only use the trace end if it wasn't too close, which results
	// in visually bizarre forward angles
	//if ( tr.fraction > 0.1 || bUseHitboxes )
	if ( tr.fraction > 0.1 )
	{
		VectorAngles( tr.endpos - *vecSrc, *angForward );
	}
	else
	{
		VectorAngles( endPos - *vecSrc, *angForward );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Fire a rocket
//-----------------------------------------------------------------------------
CBaseEntity *CTFWeaponBaseGun::FireRocket( CTFPlayer *pPlayer )
{
	PlayWeaponShootSound();

	// Server only - create the rocket.
#ifdef GAME_DLL
	Vector vecSrc;
	Vector vecOffset( 0.0f, 0.0f, 0.0f );
	QAngle angForward;

	int isQuakeRL = 0;
	CALL_ATTRIB_HOOK_INT( isQuakeRL, centerfire_projectile );

	if( isQuakeRL > 0 )
	{
		vecOffset.z = -3.0f;
	}
	else
	{
		vecOffset.x = 23.5f;
		vecOffset.y = 12.0f;
		vecOffset.z = -3.0f;
	}

	if ( pPlayer->GetFlags() & FL_DUCKING )
	{
		vecOffset.z = 8.0f;
	}
	GetProjectileFireSetup( pPlayer, vecOffset, &vecSrc, &angForward, false );

	CTFProjectile_Rocket *pProjectile = CTFProjectile_Rocket::Create( this, vecSrc, angForward, pPlayer, pPlayer );
	if ( pProjectile )
	{
		pProjectile->SetCritical( IsCurrentAttackACrit() );
		pProjectile->SetDamage( GetProjectileDamage() );
	}
	return pProjectile;

#endif

	return NULL;
}

//-----------------------------------------------------------------------------
// Purpose: Fire a projectile nail
//-----------------------------------------------------------------------------
CBaseEntity *CTFWeaponBaseGun::FireNail( CTFPlayer *pPlayer, int iSpecificNail )
{
	PlayWeaponShootSound();

	Vector vecSrc;
	QAngle angForward;
	GetProjectileFireSetup( pPlayer, Vector(16,6,-8), &vecSrc, &angForward );

	// Add some spread
	float flSpread = 1.5;

	angForward.x += RandomFloat(-flSpread, flSpread);
	angForward.y += RandomFloat(-flSpread, flSpread);

	CTFBaseProjectile *pProjectile = NULL;
	switch( iSpecificNail )
	{
	case TF_PROJECTILE_SYRINGE:
		pProjectile = CTFProjectile_Syringe::Create( vecSrc, angForward, pPlayer, pPlayer, IsCurrentAttackACrit() );
		break;

	default:
		Assert(0);
	}

	if ( pProjectile )
	{
		pProjectile->SetWeaponID( GetWeaponID() );
		pProjectile->SetCritical( IsCurrentAttackACrit() );
#ifdef GAME_DLL
		pProjectile->SetDamage( GetProjectileDamage() );
#endif
	}
	
	return pProjectile;
}

//-----------------------------------------------------------------------------
// Purpose: Fire a  pipe bomb
//-----------------------------------------------------------------------------
CBaseEntity *CTFWeaponBaseGun::FirePipeBomb( CTFPlayer *pPlayer, bool bRemoteDetonate )
{
	PlayWeaponShootSound();

#ifdef GAME_DLL

	int iMode = TF_GL_MODE_REGULAR, iNoSpin = 0;
	CALL_ATTRIB_HOOK_INT( iMode, set_detonate_mode );

	if ( bRemoteDetonate )
	{
		iMode = TF_GL_MODE_REMOTE_DETONATE;
	}

	Vector vecForward, vecRight, vecUp;
	AngleVectors( pPlayer->EyeAngles(), &vecForward, &vecRight, &vecUp );

	// Create grenades here!!
	Vector vecSrc = pPlayer->Weapon_ShootPosition();
	vecSrc +=  vecForward * 16.0f + vecRight * 8.0f + vecUp * -6.0f;
	
	Vector vecVelocity = ( vecForward * GetProjectileSpeed() ) + ( vecUp * 200.0f ) + ( random->RandomFloat( -10.0f, 10.0f ) * vecRight ) +		
		( random->RandomFloat( -10.0f, 10.0f ) * vecUp );

	float flDamageMult = 1.0f;
	CALL_ATTRIB_HOOK_FLOAT( flDamageMult, mult_dmg );

	CALL_ATTRIB_HOOK_INT ( iNoSpin, grenade_no_spin );

	AngularImpulse spin( 600, 0, 0 );

	if ( iNoSpin == 0 )
	{
		spin = AngularImpulse( 600, random->RandomInt( -1200, 1200 ), 0 );
	}

	CTFGrenadePipebombProjectile *pProjectile = CTFGrenadePipebombProjectile::Create( vecSrc, pPlayer->EyeAngles(), vecVelocity, 
		spin, pPlayer, GetTFWpnData(), iMode, flDamageMult, this );


	if ( pProjectile )
	{
		pProjectile->SetCritical( IsCurrentAttackACrit() );
	}
	return pProjectile;

#endif

	return NULL;
}

//-----------------------------------------------------------------------------
// Purpose: Fire a flare
//-----------------------------------------------------------------------------
CBaseEntity *CTFWeaponBaseGun::FireFlare( CTFPlayer *pPlayer )
{
	PlayWeaponShootSound();

#ifdef GAME_DLL
	Vector vecSrc;
	QAngle angForward;
	Vector vecOffset( 23.5f, 12.0f, -3.0f );
	if ( pPlayer->GetFlags() & FL_DUCKING )
	{
		vecOffset.z = 8.0f;
	}
	GetProjectileFireSetup( pPlayer, vecOffset, &vecSrc, &angForward, false );


	CTFProjectile_Flare *pProjectile = CTFProjectile_Flare::Create( this, vecSrc, angForward, pPlayer, pPlayer );
	if ( pProjectile )
	{
		pProjectile->SetCritical( IsCurrentAttackACrit() );
		pProjectile->SetDamage( GetProjectileDamage() );
	}
	return pProjectile;
#endif

	return NULL;
}

//-----------------------------------------------------------------------------
// Purpose: Fire a throwable
//-----------------------------------------------------------------------------
CBaseEntity *CTFWeaponBaseGun::FireJar( CTFPlayer *pPlayer, int iType )
{
	PlayWeaponShootSound();

#ifdef GAME_DLL
	AngularImpulse spin = AngularImpulse( 600, random->RandomInt( -1200, 1200 ), 0 );

	Vector vecForward, vecRight, vecUp;
	AngleVectors( pPlayer->EyeAngles(), &vecForward, &vecRight, &vecUp );

	// Set the starting position a bit behind the player so the projectile
	// launches out of the players view
	Vector vecSrc = pPlayer->Weapon_ShootPosition();
	vecSrc +=  vecForward * -64.0f + vecRight * 8.0f + vecUp * -6.0f;
	
	Vector vecVelocity = ( vecForward * GetProjectileSpeed() ) + ( vecUp * 200.0f ) + ( random->RandomFloat( -10.0f, 10.0f ) * vecRight ) +		
		( random->RandomFloat( -10.0f, 10.0f ) * vecUp );

	//GetProjectileFireSetup( pPlayer, vecOffset, &vecSrc, &angForward, false, false );

	CTFWeaponBaseGrenadeProj *pProjectile = NULL;

	switch ( iType )
	{
	case TF_PROJECTILE_JAR:
	case TF_PROJECTILE_FESTITIVE_URINE:
	case TF_PROJECTILE_BREADMONSTER_JARATE:
		pProjectile = CTFProjectile_Jar::Create(this, vecSrc, pPlayer->EyeAngles(), vecVelocity, pPlayer, pPlayer, spin, GetTFWpnData() );
		break;
	case TF_PROJECTILE_JAR_MILK:
	case TF_PROJECTILE_BREADMONSTER_MADMILK:
		pProjectile = CTFProjectile_JarMilk::Create(this, vecSrc, pPlayer->EyeAngles(), vecVelocity, pPlayer, pPlayer, spin, GetTFWpnData() );
		break;
	}
	
	if ( pProjectile )
	{
		pProjectile->SetCritical( IsCurrentAttackACrit() );
		pProjectile->SetDamage( GetProjectileDamage() );
	}
	return pProjectile;
#endif

	return NULL;
}

//-----------------------------------------------------------------------------
// Purpose: Fire an Arrow
//-----------------------------------------------------------------------------
CBaseEntity *CTFWeaponBaseGun::FireArrow( CTFPlayer *pPlayer, int iType )
{
	PlayWeaponShootSound();

#ifdef GAME_DLL
	Vector vecSrc;
	QAngle angForward;
	Vector vecOffset( 23.5f, 12.0f, -3.0f );
	if ( pPlayer->GetFlags() & FL_DUCKING )
	{
		vecOffset.z = 8.0f;
	}
	/*if ( IsWeapon( TF_WEAPON_COMPOUND_BOW ) )
	{
		// Valve were apparently too lazy to fix the viewmodel and just flipped it through the code.
		vecOffset.y *= -1.0f;
	}*/
	GetProjectileFireSetup( pPlayer, vecOffset, &vecSrc, &angForward, false, true );

	CTFProjectile_Arrow *pProjectile = CTFProjectile_Arrow::Create( this, vecSrc, angForward, GetProjectileSpeed(), GetProjectileGravity(), IsFlameArrow(), pPlayer, pPlayer, iType );
	if ( pProjectile )
	{
		pProjectile->SetCritical( IsCurrentAttackACrit() );
		pProjectile->SetDamage( GetProjectileDamage() );
	}
	return pProjectile;
#endif

	return NULL;
}

//-----------------------------------------------------------------------------
// Purpose: Use this for any old grenades: MIRV, Frag, etc
//-----------------------------------------------------------------------------
CBaseEntity *CTFWeaponBaseGun::FireGrenade( CTFPlayer *pPlayer )
{
	PlayWeaponShootSound();

#ifdef GAME_DLL

	Vector vecForward, vecRight, vecUp;
	AngleVectors( pPlayer->EyeAngles(), &vecForward, &vecRight, &vecUp );

	// Create grenades here!!
	Vector vecSrc = pPlayer->Weapon_ShootPosition();
	vecSrc += vecForward * 16.0f + vecRight * 8.0f + vecUp * -6.0f;

	Vector vecVelocity = ( vecForward * GetProjectileSpeed() ) + ( vecUp * 200.0f ) + ( random->RandomFloat( -10.0f, 10.0f) * vecRight ) +
		( random->RandomFloat( -10.0f, 10.0f ) * vecUp );

	float flDamageMult = 1.0f;
	CALL_ATTRIB_HOOK_FLOAT( flDamageMult, mult_dmg );

	char szEntName[256];
	V_snprintf( szEntName, sizeof( szEntName ), "%s_projectile", WeaponIdToClassname( GetWeaponID() ) );

	CTFWeaponBaseGrenadeProj *pProjectile = CTFWeaponBaseGrenadeProj::Create( szEntName, vecSrc, pPlayer->EyeAngles(), vecVelocity,
		AngularImpulse( 600, random->RandomInt( -1200, 1200 ), 0 ),
		pPlayer, GetTFWpnData(), 3.0f, flDamageMult );


	if ( pProjectile )
	{
		pProjectile->SetCritical( IsCurrentAttackACrit() );
		pProjectile->SetLauncher( this );
	}
	return pProjectile;

#endif

	return NULL;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFWeaponBaseGun::PlayWeaponShootSound( void )
{
	if ( IsCurrentAttackACrit() )
	{
		WeaponSound( BURST );
	}
	else
	{
		WeaponSound( SINGLE );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int CTFWeaponBaseGun::GetAmmoPerShot( void ) const
{
	int iAmmoPerShot = m_pWeaponInfo->GetWeaponData( m_iWeaponMode ).m_iAmmoPerShot;
	CALL_ATTRIB_HOOK_INT( iAmmoPerShot, mod_ammo_per_shot );
	return iAmmoPerShot;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFWeaponBaseGun::RemoveAmmo( CTFPlayer *pPlayer )
{
#ifndef CLIENT_DLL
	if (UsesClipsForAmmo1())
	{
		m_iClip1 -= GetAmmoPerShot();
	}
	else
	{
		if (m_iWeaponMode == TF_WEAPON_PRIMARY_MODE)
		{
			pPlayer->RemoveAmmo( GetAmmoPerShot(), m_iPrimaryAmmoType );
			if (0 < m_iRefundedAmmo)
				pPlayer->GiveAmmo( m_iRefundedAmmo, m_iPrimaryAmmoType );
		}
		else
		{
			pPlayer->RemoveAmmo( GetAmmoPerShot(), m_iSecondaryAmmoType );
			if (0 < m_iRefundedAmmo)
				pPlayer->GiveAmmo( m_iRefundedAmmo, m_iSecondaryAmmoType );
		}

		m_iRefundedAmmo = 0;
	}
#endif
}
//-----------------------------------------------------------------------------
// Purpose: Accessor for damage, so sniper etc can modify damage
//-----------------------------------------------------------------------------
float CTFWeaponBaseGun::GetProjectileSpeed( void )
{
	// placeholder for now
	// grenade launcher and pipebomb launcher hook this to set variable pipebomb speed

	return 0;
}

//-----------------------------------------------------------------------------
// Purpose: Accessor for gravity, so sniper etc can modify gravity
//-----------------------------------------------------------------------------
float CTFWeaponBaseGun::GetProjectileGravity(void)
{
	return 0.001f;
}

//-----------------------------------------------------------------------------
// Purpose: Accessor for huntsman flame arrow
//-----------------------------------------------------------------------------
bool CTFWeaponBaseGun::IsFlameArrow(void)
{
	return false;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
float CTFWeaponBaseGun::GetWeaponSpread( void )
{
	float flSpread = m_pWeaponInfo->GetWeaponData( m_iWeaponMode ).m_flSpread;
	CALL_ATTRIB_HOOK_FLOAT( flSpread, mult_spread_scale );
	return flSpread;
}

//-----------------------------------------------------------------------------
// Purpose: Accessor for damage, so sniper etc can modify damage
//-----------------------------------------------------------------------------
float CTFWeaponBaseGun::GetProjectileDamage( void )
{
	float flDamage = (float)m_pWeaponInfo->GetWeaponData( m_iWeaponMode ).m_nDamage;
	CALL_ATTRIB_HOOK_FLOAT( flDamage, mult_dmg );
	return flDamage;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
bool CTFWeaponBaseGun::Holster( CBaseCombatWeapon *pSwitchingTo )
{
// Server specific.
#if !defined( CLIENT_DLL )

	// Make sure to zoom out before we holster the weapon.
	ZoomOut();
	SetContextThink( NULL, 0, ZOOM_CONTEXT );

#endif

	return BaseClass::Holster( pSwitchingTo );
}

//-----------------------------------------------------------------------------
// Purpose:
// NOTE: Should this be put into fire gun
//-----------------------------------------------------------------------------
void CTFWeaponBaseGun::DoFireEffects()
{
	CTFPlayer *pPlayer = ToTFPlayer( GetPlayerOwner() );
	if ( !pPlayer )
		return;

	// Muzzle flash on weapon.
	bool bMuzzleFlash = true;

	// We no longer need this
	/*
	if ( pPlayer->IsPlayerClass( TF_CLASS_HEAVYWEAPONS ) )
	{
		//CTFWeaponBase *pWeapon = pPlayer->GetActiveTFWeapon();
		//if ( pWeapon && pWeapon->GetWeaponID() == TF_WEAPON_MINIGUN )
		if (pPlayer->IsActiveTFWeapon(TF_WEAPON_MINIGUN))
		{
			bMuzzleFlash = false;
		}
	}*/

	if ( pPlayer->IsPlayerClass( TF_CLASS_SNIPER ) )
	{
		if ( pPlayer->IsActiveTFWeapon( TF_WEAPON_COMPOUND_BOW ) )
		{
			bMuzzleFlash = false;
		}
	}

	if ( bMuzzleFlash )
	{
		pPlayer->DoMuzzleFlash();
	}
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CTFWeaponBaseGun::ToggleZoom( void )
{
	// Toggle the zoom.
	CBasePlayer *pPlayer = GetPlayerOwner();
	if ( pPlayer )
	{
		if( pPlayer->GetFOV() >= 75 )
		{
			ZoomIn();
		}
		else
		{
			ZoomOut();
		}
	}

	// Get the zoom animation time.
	m_flNextSecondaryAttack = gpGlobals->curtime + 1.2;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CTFWeaponBaseGun::ZoomIn( void )
{
	// The the owning player.
	CTFPlayer *pPlayer = ToTFPlayer( GetPlayerOwner() );
	if ( !pPlayer )
		return;

	// Set the weapon zoom.
	// TODO: The weapon fov should be gotten from the script file.
	pPlayer->SetFOV( pPlayer, TF_WEAPON_ZOOM_FOV, 0.1f );
	pPlayer->m_Shared.AddCond( TF_COND_ZOOMED );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CTFWeaponBaseGun::ZoomOut( void )
{
	// The the owning player.
	CTFPlayer *pPlayer = ToTFPlayer( GetPlayerOwner() );
	if ( !pPlayer )
		return;

	if ( pPlayer->m_Shared.InCond( TF_COND_ZOOMED ) )
	{
		// Set the FOV to 0 set the default FOV.
		pPlayer->SetFOV( pPlayer, 0, 0.1f );
		pPlayer->m_Shared.RemoveCond( TF_COND_ZOOMED );
	}
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CTFWeaponBaseGun::ZoomOutIn( void )
{
	//Zoom out, set think to zoom back in.
	ZoomOut();
	SetContextThink( &CTFWeaponBaseGun::ZoomIn, gpGlobals->curtime + ZOOM_REZOOM_TIME, ZOOM_CONTEXT );
}
