/**
 * Copyright (C) 2009 - 2010, Vrai Stacey.
 * 
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 * 
 *     http://www.apache.org/licenses/LICENSE-2.0
 *     
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include "fudge/types.h"
#include "registry_internal.h"

fudge_bool FudgeType_typeIsFixedWidth ( fudge_type_id type )
{
    return FudgeRegistry_getTypeDesc ( type )->fixedwidth >= 0;
}

fudge_i32 FudgeType_getFixedWidth ( fudge_type_id type )
{
    return FudgeRegistry_getTypeDesc ( type )->fixedwidth;
}

