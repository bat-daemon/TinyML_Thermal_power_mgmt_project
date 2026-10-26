/**
  ******************************************************************************
  * @file    network_data_params.c
  * @author  AST Embedded Analytics Research Platform
  * @date    2026-10-26T22:59:52+0530
  * @brief   AI Tool Automatic Code Generator for Embedded NN computing
  ******************************************************************************
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  ******************************************************************************
  */

#include "network_data_params.h"


/**  Activations Section  ****************************************************/
ai_handle g_network_activations_table[1 + 2] = {
  AI_HANDLE_PTR(AI_MAGIC_MARKER),
  AI_HANDLE_PTR(NULL),
  AI_HANDLE_PTR(AI_MAGIC_MARKER),
};




/**  Weights Section  ********************************************************/
AI_ALIGNED(32)
const ai_u64 s_network_weights_array_u64[251] = {
  0xbf411df2be5a5651U, 0xbd8bd3a6bf3779d3U, 0xbe2f6d49beee4477U, 0x3e316f963f0da003U,
  0xbe977b44bed75eb2U, 0xbecbb6c6beb85e5aU, 0x3f1516343f959ab2U, 0x3ed1654dbfbbf55dU,
  0x3f3700433de9aef5U, 0xbe4651323eb5fcc3U, 0x3f217aac3e191eceU, 0xbed6ca41bf5abc9aU,
  0x3d31cfa83dd0cccbU, 0xbeb356b5be969fabU, 0x3ef33fde3f919bbaU, 0x3e8ba5f1bf05423bU,
  0xbe9ceb613ca697c1U, 0xbed64a51bf177132U, 0x3f1c5f9d3f78cdeeU, 0x3d1679fe3f07f711U,
  0xbe006f1a3e9fdf25U, 0x3ec84e363efdaac6U, 0x3f56729bbe2055b0U, 0x3de3d43bbea7ae83U,
  0xbfd7ff163f77992eU, 0xbedf408bbe9309d9U, 0x3dbec6bbbed33c40U, 0xbed360323e11cec6U,
  0xbda81f35beca7498U, 0xbec230bc3c2e67ebU, 0xbd4737493ebed554U, 0x3ef146c8bf6682f3U,
  0x3d88b6d2bf1aa0f4U, 0x3e2688fe3db6e850U, 0xbf8b6006bddfaf75U, 0x3dc68a3b3fbaa747U,
  0xbef6640abdb4d57aU, 0x3d3112ccbf01c335U, 0x3ec2ad7b3f4b28ffU, 0x3e863cde3c4554c5U,
  0xbeb9a63f3e1a0ccaU, 0x3f857d8cbcb62d3fU, 0xbefb68b53e844298U, 0x3e9417e4be858b43U,
  0x3ebcbe34beed06dbU, 0x3f0345c13f3fa5e7U, 0x3ea4700abf6a9b05U, 0x3e5447f7be9435a4U,
  0xbee402803e41bdb1U, 0x3dd112563f4ccca5U, 0xbf2fc317bb351eb2U, 0x3eb18a6b3eb08949U,
  0xbe8ae450bf3b9c7eU, 0xbefabf923ce9b419U, 0x3f152857bec42fc4U, 0xbf0ae01e3db05adcU,
  0x3d297ed6bdda9a3fU, 0xbd6d2d143dc32b93U, 0xbe2022afbebcc33fU, 0x3da9298dbea3b9b9U,
  0xbefa8c5d3d9fe8a3U, 0xbe752ea03f72cef1U, 0x3ec724843e8fa73fU, 0x3ec6fcc33e84ed3eU,
  0xbf48de1bbb277f24U, 0xbea228bbbf008dc0U, 0x3b687b813f9b90a2U, 0xbf6bf0bb3ef88613U,
  0x3f98c0a9bf371f2dU, 0xbe3cb78d3f2ddfc9U, 0xbe102fba3efded78U, 0xbe754d7f3eb6d626U,
  0x3df0eb803dfbfa87U, 0x3e82baa5bec2aa47U, 0x3ed50a9f3e9fbc63U, 0xbf0fec373f20fe0bU,
  0x3e9b2c75bf0334c9U, 0x3d99b8e03fb1a6eeU, 0x3e9a11f83f382adbU, 0x3e7070e73ef5731eU,
  0xbeed042c3ebe5310U, 0x3f3de5d7bd0cee3eU, 0xbbc756dc3e60fd3bU, 0xbf15824ebf10c059U,
  0x3edfb12fbd725c04U, 0xbeeb232d3f8319a1U, 0xbdac0b74bed97720U, 0xbe80d4b43f90a27aU,
  0x3eba9c943de3ac0aU, 0x3e9db8d9bde11677U, 0xbd86b96a3e14cde6U, 0xbf2fc537be565151U,
  0xbf09fc4abe8aecd4U, 0x3e371c933f83716bU, 0x3ed85b7f3ecc137dU, 0x3d0f68343e1aeafcU,
  0xbe54b9bbbf9360f5U, 0xbf1d85ffbd1d44abU, 0x3e1f8a4fbdf727c1U, 0xbf38775c3f1dbbe6U,
  0xbfc17847be823f0dU, 0x3f0274983f46780eU, 0xbe8eb8b13ea24404U, 0xbf8fae16beffcdfeU,
  0x3ebe9cd83d1b368dU, 0x3e8bcf103d719eadU, 0x3f007c19bfb73e01U, 0x3e94ab53bf99d19dU,
  0xbecbf2df3e16bdf6U, 0xbfd037ae3edc0b5eU, 0xbe0898bebf9086fbU, 0xbdc2be673eec7ef7U,
  0xbe37af38bf0246aaU, 0x3f5f623c3fb179c1U, 0x3ebf0b33bf047a44U, 0xbe811474be33c911U,
  0xbf996265be3b2698U, 0xbf4e2cd93dfef8d2U, 0xbe5f848039b42ab2U, 0x3d4e2efcbeb14ad7U,
  0x3e62f0c63ee585f3U, 0x3ed462e23df45aedU, 0x3ed312543ed7cc35U, 0xbeaf8d1bbf8ef9f3U,
  0xbe85b90bbfce8b1aU, 0xbf803bf1bdc01561U, 0x3eda39dc3e4c4963U, 0xbf2687f4be91b220U,
  0xbd4e34b0bf4eaf05U, 0x3d1afe183f118182U, 0x3ef967933e965861U, 0xbee0f767be511259U,
  0xbf33e691bf3bb9caU, 0xbe7172283ec70af2U, 0xbfbb8a55be543952U, 0xbf2ac209bf6b29bfU,
  0x3f13b8b33b84f234U, 0x3ebb387f3dd26cfaU, 0x3ec579263f62924bU, 0x3d6b83ebbf26ca53U,
  0x3f80e19f3e9dc667U, 0xbe9835eebf5de856U, 0x3f0bc890bd8d5f4aU, 0x3f000ead3eb0f8beU,
  0xbe45f9fbbf21aaf6U, 0x3f13ce283f3e6518U, 0xbe2083943e5afd95U, 0xbe21b8c3bea5f0b2U,
  0xbe8780a3b9edbd2aU, 0xbeb93eaa3ee28fd5U, 0x3e9159d33e50b966U, 0x3e8d8acb3e518253U,
  0x3e2e7e053fa1aaa0U, 0xbf294fd73e94cf99U, 0x3e208e2a3d3eef92U, 0x3f3414b33e90a153U,
  0xbf5a42f5bff27d04U, 0x3e8c949c3e60c498U, 0xbf8ee6433e18535fU, 0xbf5d0babbf4651a3U,
  0x3ed80cd2be8458ebU, 0x3efd1c98bf4c0aa6U, 0x3f0d66de3eb8657cU, 0xbe520905bd17b614U,
  0x3f9e87af3c5c0064U, 0x3de133783e62157fU, 0x3fa40b123da24035U, 0x3e43d1ae3ef60b80U,
  0x3d5cf238be722b4fU, 0x3f0df3563f459ef0U, 0xbe7d8808bf58c612U, 0x3dc999cbbf13e2beU,
  0xbfaa0c13bd515892U, 0xbed4d7723e9d9a25U, 0x3ed2e2413ebc21efU, 0xbe54261bbef99cebU,
  0x3e9e57523f391bb0U, 0xbebf95eabee6a176U, 0x3ef266f83f19168bU, 0xbecb81703f080e37U,
  0xbfbf0693bebf274bU, 0x3f510bf23e6a51f6U, 0xbf1a9d6c3ef28561U, 0xbfea3ac0beb53513U,
  0x3ee313763f4c3971U, 0x3aa18f0a3eb21b91U, 0x3f13bb7ebf7267e2U, 0x3ebe3b373db86651U,
  0x3f01e552be697242U, 0x3b209103bffe9f46U, 0x3e9ea6f6bcf6faaeU, 0x3eb4bb38beb5c36eU,
  0x3ea85301be8edcffU, 0x3f2531be3ec12e7aU, 0xbdce7a91be3b3146U, 0xbe1e84253f426e44U,
  0x3e1665793e011c19U, 0x3e4b65873d91a2f2U, 0x3caea80d3e5b4e55U, 0x3e877396be265cbbU,
  0xbc65628abd497c28U, 0xbf3cf2293e8e50f9U, 0xbec8d2033e93ff9fU, 0xc0016bd1be4898d6U,
  0x3f33ad0dbf898b8aU, 0xbfba86f9bd1cd048U, 0x3e8247953f2c32a3U, 0x3e42e9963e22fa3dU,
  0x3e2bfea33e667015U, 0x3e992d443e4b8f58U, 0x3f14db8dbee8152eU, 0x403da1e4beac993fU,
  0xbf10af633d0dfaccU, 0xbe4922073ea25e14U, 0x3f50d2c13c7fd79cU, 0xbefc89f9bf3fa1e8U,
  0x3ef0cc96bdd68858U, 0xbed0d99e3d2ce63fU, 0xbf1f73d03e08e0cbU, 0xbe67d2d1bf53fd4eU,
  0xbe70d5c7bc912b89U, 0x3ed8617bbf18de4dU, 0xbedff1edbe3b0caeU, 0xbf32e78f3f17517fU,
  0x3e0660043da09746U, 0x3f74be59be4999b2U, 0xbf772e49c01b8098U, 0xbf90f8fdbff6aa23U,
  0x3f8355e03f366796U, 0xbf2f65fd3e799901U, 0xbf4ce1ee3eb627e8U, 0x3f06977abf197506U,
  0xbc753e81bf0e66cdU, 0xbffa06c63f38af86U, 0x3f9b5c813f530174U, 0xbe29912c3f4509f7U,
  0x3e5546a43e897d75U, 0xbcfb2b27bf32a96eU, 0x3cdda3e43df560ffU, 0xbc03dd6cbe602651U,
  0xbf91eb9e3f83e637U, 0x3ffbc638bd793019U, 0x3da3b9f03f34cbcbU, 0x3f8d1dd83f1dac6aU,
  0xbff79a843f20d703U, 0xbebdf9e43f2ba16bU, 0xbf5ddda8bf3abfacU, 0x3f5c7546beaeeae1U,
  0x3f8a3dc4bf729d09U, 0x3cb3f25cc01bc56dU, 0x3c17cd663c070b24U,
};


ai_handle g_network_weights_table[1 + 2] = {
  AI_HANDLE_PTR(AI_MAGIC_MARKER),
  AI_HANDLE_PTR(s_network_weights_array_u64),
  AI_HANDLE_PTR(AI_MAGIC_MARKER),
};

