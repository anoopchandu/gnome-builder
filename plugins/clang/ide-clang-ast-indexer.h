/* ide-clang-ast-indexer.h
 *
 * Copyright (C) 2017 Anoop Chandu <anoopchandu96@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef IDE_CLANG_AST_INDEXER_H
#define IDE_CLANG_AST_INDEXER_H

#include <ide.h>

/* This object will index list of files using their IDs and put that index 
 * into a destination file.
 */

G_BEGIN_DECLS

#define IDE_TYPE_CLANG_AST_INDEXER (ide_clang_ast_indexer_get_type())

G_DECLARE_FINAL_TYPE (IdeClangAstIndexer, ide_clang_ast_indexer, IDE, CLANG_AST_INDEXER, IdeObject)

G_END_DECLS

#endif /* IDE_CLANG_AST_INDEXER_H */