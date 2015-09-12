//=======================================================================================
// SkookumScript Plugin for Unreal Engine 4
// Copyright (c) 2015 Agog Labs Inc. All rights reserved.
//
// Binding classes for UE4 
//
// Author: Markus Breyer
//=======================================================================================

#include "../SkookumScriptRuntimePrivatePCH.h"
#include "SkUEClassBinding.hpp"
#include "../Classes/SkookumScriptComponent.h"
#include <SkUEWorld.generated.hpp>

//---------------------------------------------------------------------------------------

TMap<UClass*, SkClass*>                             SkUEClassBindingHelper::ms_static_class_map_u2s;
TMap<SkClassDescBase*, UClass*>                     SkUEClassBindingHelper::ms_static_class_map_s2u;
#if WITH_EDITORONLY_DATA
TMap<SkClassDescBase*, TWeakObjectPtr<UBlueprint>>  SkUEClassBindingHelper::ms_dynamic_class_map_s2u;
TMap<UBlueprint*, SkClass*>                         SkUEClassBindingHelper::ms_dynamic_class_map_u2s;
#endif

int32_t                                             SkUEClassBindingHelper::ms_world_data_idx = -1;

//---------------------------------------------------------------------------------------
// Compute data idx to world variable
int32_t SkUEClassBindingHelper::get_world_data_idx()
  {
  if (ms_world_data_idx < 0)
    {
    if (!SkBrain::ms_object_class_p->get_class_data().find(ASymbolX_c_world, AMatch_first_found, (uint32_t*)&ms_world_data_idx))
      {
      SK_ASSERTX(false, "Couldn't find the @@world class member variable!");
      }
    }

  return ms_world_data_idx;
  }

//---------------------------------------------------------------------------------------
// Get pointer to UWorld from global variable
UWorld * SkUEClassBindingHelper::get_world()
  {
  SkInstance * world_var_p = SkBrain::ms_object_class_p->get_class_data_value_by_idx(get_world_data_idx());
  SK_ASSERTX(world_var_p && (world_var_p == SkBrain::ms_nil_p || world_var_p->get_class() == SkBrain::get_class(ASymbol_World)), "@@world variable does not have proper type."); // nil is ok
  return world_var_p == SkBrain::ms_nil_p ? nullptr : world_var_p->as<SkUEWorld>();
  }

//---------------------------------------------------------------------------------------
// Set the game world (C++ and Skookum variable) to a specific world object
void SkUEClassBindingHelper::set_world(UWorld * world_p)
  {
  SkBrain::ms_object_class_p->set_class_data_value_by_idx_no_ref(get_world_data_idx(), world_p ? SkUEWorld::new_instance(world_p) : SkBrain::ms_nil_p);
  }

//---------------------------------------------------------------------------------------
// Determine SkookumScript class from UClass
SkClass * SkUEClassBindingHelper::get_object_class(UObject * obj_p, UClass * def_uclass_p /*= nullptr*/, SkClass * def_class_p /*= nullptr*/)
  {
  SkClass * class_p = def_class_p;

  if (obj_p)
    {
    UClass * obj_uclass_p = obj_p->GetClass();
    if (obj_uclass_p != def_uclass_p)
      {
      // Crawl up class hierarchy until we find a class known to Sk
      SkClass * obj_class_p = nullptr;
      for (; !obj_class_p && obj_uclass_p; obj_uclass_p = obj_uclass_p->GetSuperClass())
        {
        obj_class_p = get_sk_class_from_ue_class(obj_uclass_p);
        }
      SK_ASSERTX(obj_class_p, a_str_format("UObject of type '%S' has no matching SkookumScript type!", *obj_p->GetClass()->GetName()));
      class_p = obj_class_p;
      }
    }

  return class_p;
  }

//---------------------------------------------------------------------------------------
// Return instance of an actor's SkookumScriptComponent, if any
SkInstance * SkUEClassBindingHelper::get_actor_component_instance(AActor * actor_p)
  {
  // If the actor has component, return the instance contained in the component
  if (actor_p)
    {
    USkookumScriptComponent * component_p = static_cast<USkookumScriptComponent *>(actor_p->GetComponentByClass(USkookumScriptComponent::StaticClass()));
    if (component_p)
      {
      return component_p->get_instance();
      }
    }

  return nullptr;
  }

//---------------------------------------------------------------------------------------

void SkUEClassBindingHelper::reset_static_class_mappings(uint32_t reserve)
  {
  ms_static_class_map_u2s.Reset();
  ms_static_class_map_s2u.Reset();
  ms_static_class_map_u2s.Reserve(reserve);
  ms_static_class_map_s2u.Reserve(reserve);
  }

//---------------------------------------------------------------------------------------

void SkUEClassBindingHelper::add_static_class_mapping(SkClass * sk_class_p, UClass * ue_class_p)
  {
  ms_static_class_map_u2s.Add(ue_class_p, sk_class_p);
  ms_static_class_map_s2u.Add(sk_class_p, ue_class_p);
  }

#if WITH_EDITORONLY_DATA
//---------------------------------------------------------------------------------------

UClass * SkUEClassBindingHelper::add_dynamic_class_mapping(SkClassDescBase * sk_class_desc_p)
  {
  // Get fully derived SkClass
  SkClass * sk_class_p = sk_class_desc_p->get_key_class();

  // Dynamic classes have blueprints - look it up by name
  FString class_name(sk_class_p->get_name_cstr());
  UBlueprint * blueprint_p = FindObject<UBlueprint>(ANY_PACKAGE, *class_name);
  if (!blueprint_p) return nullptr;

  // Add to map of known class equivalences
  ms_dynamic_class_map_u2s.Add(blueprint_p, sk_class_p);
  ms_dynamic_class_map_s2u.Add(sk_class_p, blueprint_p);

  // Return latest generated class belonging to this blueprint
  return blueprint_p->GeneratedClass;
  }

//---------------------------------------------------------------------------------------

SkClass * SkUEClassBindingHelper::add_dynamic_class_mapping(UBlueprint * blueprint_p)
  {
  // Look up SkClass by blueprint name
  AString class_name(*blueprint_p->GetName(), blueprint_p->GetName().Len());
  SkClass * sk_class_p = SkBrain::get_class(ASymbol::create(class_name, ATerm_short));

  // If found, add to map of known class equivalences
  if (sk_class_p)
    {
    ms_dynamic_class_map_u2s.Add(blueprint_p, sk_class_p);
    ms_dynamic_class_map_s2u.Add(sk_class_p, blueprint_p);
    }

  return sk_class_p;
  }

#else // !WITH_EDITORONLY_DATA = cooked data build
//---------------------------------------------------------------------------------------

UClass * SkUEClassBindingHelper::add_static_class_mapping(SkClassDescBase * sk_class_desc_p)
  {
  // Get fully derived SkClass
  SkClass * sk_class_p = sk_class_desc_p->get_key_class();

  // Look it up and remember it
  FString class_name(sk_class_p->get_name_cstr());
  UClass * ue_class_p = FindObject<UClass>(ANY_PACKAGE, *(class_name + TEXT("_C")));
  if (ue_class_p)
    {
    add_static_class_mapping(sk_class_p, ue_class_p);
    }

  return ue_class_p;
  }

//---------------------------------------------------------------------------------------

SkClass * SkUEClassBindingHelper::add_static_class_mapping(UClass * ue_class_p)
  {
  // Look up SkClass by blueprint name
  const FString & ue_class_name = ue_class_p->GetName(); 
  int32 ue_class_name_len = ue_class_name.Len();
  if (ue_class_name_len < 3) return nullptr;

  // When we get to this function, we are looking for a class name that has "_C" appended at the end
  // So we subtract two from the length to truncate those two characters
  // We don't check here if the last two characters actually _are_ "_C" because it does not matter
  // since in that case it would be an error anyway
  AString class_name(*ue_class_name, ue_class_name_len - 2);
  SkClass * sk_class_p = SkBrain::get_class(ASymbol::create(class_name, ATerm_short));

  // If found, add to map of known class equivalences
  if (sk_class_p)
    {
    add_static_class_mapping(sk_class_p, ue_class_p);
    }

  return sk_class_p;
  }

#endif